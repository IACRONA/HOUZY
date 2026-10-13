// HOUZY sphere: WebGL2 port of the plugin's LiquidOrb::render (PluginEditor.h), GRAPHITE colours from kOrbColours.
// Shared by index.html (chapter stage) and plugin.html (the hybrid panel). Returns { st } — set st.yaw / st.pitch / st.spin / st.amp / st.speed.
(function () {
  var clamp = function (v, a, b) { return Math.max(a, Math.min(b, v)); };
  window.HouzyOrb = function (host) {
    if (!host) return null;
    var cv = document.createElement('canvas'); host.appendChild(cv);
    var gl = cv.getContext('webgl2', { premultipliedAlpha: true, alpha: true, antialias: false });
    if (!gl) {   // no WebGL: a still graphite sphere in CSS
      host.style.background = 'radial-gradient(circle at 34% 30%,#c9d2d2 0,#5a6b6b 22%,#26302f 58%,#141a1a 80%)';
      host.style.borderRadius = '50%'; host.style.transform += ' scale(.88)'; cv.remove();
      return { set: function () {}, setColours: function () {} };
    }
    var VS = '#version 300 es\nin vec2 p;void main(){gl_Position=vec4(p,0.,1.);}';
    var FS = [
      '#version 300 es', 'precision highp float;',
      'uniform vec2 uRes;uniform float uT,uAmp,uYaw,uPitch;uniform vec3 uTint,uCore,uHi;out vec4 o;',
      'const float EXT=1.14,FR=1.25,QS=4.0;float cy,sy,cp,sp;',
      'vec3 toObj(vec3 v){float x1=cy*v.x-sy*v.z,z1=sy*v.x+cy*v.z;return vec3(x1,cp*v.y+sp*z1,-sp*v.y+cp*z1);}',
      // disp(): the mint field verbatim from LiquidOrb::render
      'float disp(vec3 v){vec3 q=QS*normalize(v);',
      ' float uu=q.x*.85+q.y*.5+q.z*.2+.25*sin(q.y*1.1+uT*.28)+.15*sin(q.z*1.9-uT*.30);',
      ' float vv=q.z*.7+q.x*.55+q.y*.45+.22*sin(q.x*1.3-uT*.30)+.13*sin(q.y*2.0+uT*.32);',
      ' return .026*sin(uu*9.*FR)+.022*sin(vv*8.5*FR);}',
      // dispGrad(): its exact gradient in q
      'vec3 dispGrad(vec3 v){vec3 q=v*4.;float a1=q.y*1.1+uT*.28,b1=q.z*1.9-uT*.30,c1=q.x*1.3-uT*.30,d1=q.y*2.0+uT*.32;',
      ' float U=q.x*.85+q.y*.5+q.z*.2+.25*sin(a1)+.15*sin(b1);float V=q.z*.7+q.x*.55+q.y*.45+.22*sin(c1)+.13*sin(d1);',
      ' float k1=9.*FR,k2=8.5*FR;float cu=.026*k1*cos(U*k1),cv=.022*k2*cos(V*k2);',
      ' return vec3(cu*.85+cv*(.55+.286*cos(c1)),cu*(.5+.275*cos(a1))+cv*(.45+.26*cos(d1)),cu*(.2+.285*cos(b1))+cv*.7);}',
      'void main(){',
      ' cy=cos(uYaw);sy=sin(uYaw);cp=cos(uPitch);sp=sin(uPitch);',
      ' vec2 uv=gl_FragCoord.xy/uRes;float ix=uv.x*2.*EXT-EXT,iy=uv.y*2.*EXT-EXT;',
      // rippled silhouette: radius 1 + crest, soft-limited (tanh) into a 0.10 gap
      ' float th=atan(iy,ix);float R=1.+.10*tanh(disp(toObj(vec3(cos(th),sin(th),0.)))*uAmp/.10);',
      ' vec2 u=vec2(ix,iy)/R;float aaW=1.6*(2.*EXT)/uRes.x/R;float r=length(u);',
      ' if(r>1.+aaW){o=vec4(0.);return;}',
      ' float rc=min(r,1.);vec3 w=vec3(r>1e-6?u*rc/r:vec2(0.),sqrt(max(0.,1.-rc*rc)));',
      ' vec3 p=toObj(w);vec3 g=dispGrad(p);float cover=clamp((1.-r)/aaW+.5,0.,1.);',
      ' vec3 t1=vec3(-p.z,0.,p.x);if(dot(t1,t1)<1e-6)t1=vec3(1.,0.,0.);t1=normalize(t1);vec3 t2=cross(p,t1);',
      ' float g1=QS*uAmp*dot(g,t1),g2=QS*uAmp*dot(g,t2);vec3 n=normalize(p-g1*t1-g2*t2);',
      ' float ny1=cp*n.y-sp*n.z,nz1=sp*n.y+cp*n.z;vec3 N=vec3(cy*n.x+sy*nz1,ny1,-sy*n.x+cy*nz1);float ndv=clamp(N.z,0.,1.);',
      ' float eta=1./1.45;float k=1.-eta*eta*(1.-ndv*ndv);float fac=eta*ndv-sqrt(max(0.,k));',
      ' vec3 rv=normalize(vec3(fac*N.x,fac*N.y,-eta+fac*N.z));float dd=dot(w,rv);vec3 bb=normalize(w+rv*(-2.*dd));',
      ' vec3 L1=normalize(vec3(-.45,.85,.55)),L2=normalize(vec3(.5,.3,.75));vec3 H1=normalize(L1+vec3(0,0,1)),H2=normalize(L2+vec3(0,0,1));',
      ' float bDiff=dot(bb,L1)*.325+.325+dot(bb,L2)*.2+.2;float om=1.-ndv;float fres=om*om*om*sqrt(om);',
      ' vec3 tint=uTint*2.;vec3 c=tint*(.2+bDiff)+tint*fres*.55;',
      // core != white: light absorbed through the body (exp(-4.5(1-core)t)); the depth is an approximation of the plugin's march
      ' c*=exp(-4.5*(1.-uCore)*(.35+.45*w.z));',
      ' float graze=clamp(1.-(2.*w.z)/2.5,0.,1.);float ex1=mix(380.,90.,graze),ex2=mix(240.,60.,graze);',
      ' float h1=max(0.,dot(N,H1)),h2=max(0.,dot(N,H2));',
      ' float s1=h1>.9?pow(h1,ex1):0.,s2=h2>.9?pow(h2,ex2):0.,gl0=h1>.75?pow(h1,40.)*.1:0.;',
      ' c+=(s1*6.5+s2*3.+gl0)*uHi;c/=(1.+c*.65);o=vec4(c*cover,cover);}'
    ].join('\n');
    function sh(type, src) { var s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s); return s; }
    // The shader compiles in the background where the browser can (KHR_parallel_shader_compile): asking for the link
    // status straight away made the page wait ~0.4 s on a phone. The sphere switches on once it is ready.
    var par = gl.getExtension('KHR_parallel_shader_compile');
    var pr = gl.createProgram(); gl.attachShader(pr, sh(gl.VERTEX_SHADER, VS)); gl.attachShader(pr, sh(gl.FRAGMENT_SHADER, FS)); gl.linkProgram(pr);
    var st = { t: 0, yaw: 0, pitch: 0, amp: 1.24, speed: 0.92, spin: 0 }, visible = true, last = 0, ready = false, U = {};
    // colours as kOrbColours in PluginEditor.cpp: tint, core (absorption), highlight. GRAPHITE by default.
    var rgb = function (h) { return [(h >> 16 & 255) / 255, (h >> 8 & 255) / 255, (h & 255) / 255]; };
    var colours = [0x5A6B6B, 0xFFFFFF, 0xFFFFFF];
    function setColours(tint, core, hi) {
      colours = [tint, core, hi]; if (!ready) return;
      gl.uniform3fv(U.uTint, rgb(tint)); gl.uniform3fv(U.uCore, rgb(core)); gl.uniform3fv(U.uHi, rgb(hi));
    }
    function size() {
      if (!ready) return;
      var s = host.getBoundingClientRect().width, px = Math.round(clamp(s * Math.min(window.devicePixelRatio || 1, 2), 64, 1100));
      if (cv.width !== px) { cv.width = cv.height = px; gl.viewport(0, 0, px, px); gl.uniform2f(U.uRes, px, px); }
    }
    function setup() {
      if (!gl.getProgramParameter(pr, gl.LINK_STATUS)) { console.warn('orb shader', gl.getProgramInfoLog(pr)); return; }
      gl.useProgram(pr);
      var buf = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, buf);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);
      var loc = gl.getAttribLocation(pr, 'p'); gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
      ['uRes', 'uT', 'uAmp', 'uYaw', 'uPitch', 'uTint', 'uCore', 'uHi'].forEach(function (n) { U[n] = gl.getUniformLocation(pr, n); });
      ready = true;
      setColours(colours[0], colours[1], colours[2]);
      size();
    }
    window.addEventListener('resize', size);
    new IntersectionObserver(function (e) { visible = e[0].isIntersecting; }).observe(host);
    function frame(now) {
      if (!ready && (!par || gl.getProgramParameter(pr, par.COMPLETION_STATUS_KHR))) setup();
      var dt = last ? Math.min(0.05, (now - last) / 1000) : 0; last = now;
      if (ready && visible && !document.hidden) {
        // AMOUNT 15 % in the plugin: time speed 0.8 + 0.8·0.15, amplitude 1.6·(0.70 + 0.50·0.15); spin deepens it like compression does
        st.t += dt * st.speed * (1 + st.spin * 1.5);
        gl.uniform1f(U.uT, st.t); gl.uniform1f(U.uAmp, st.amp * (1 + 0.3 * st.spin));
        gl.uniform1f(U.uYaw, st.yaw); gl.uniform1f(U.uPitch, st.pitch);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      }
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
    return { st: st, setColours: setColours };
  };

})();
