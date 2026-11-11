// FLURIK - the plugin's live theme on the site (10.11.2026, owner: "the background must flow as in the plugin").
// The same 21st.dev "Waves" shader the plugin runs (Source/ThemeGpu.h, kFS - verbatim), same uniforms: 740 x 500,
// time x 0.37 (speed 14), vignette 0.25, grain 0.03, cursor spotlight strength 0.20 / radius 0.39, presence eased.
// The panel's chrome for theme 0 is baked with a transparent background and panes (harness HOUZY_WEBKEY), so this
// canvas, under the picture, shows through exactly where the plugin shows its live frame.
// Simplification: the plugin draws the panes from a grain-free pass; here one pass (grain 3 % under the panes too).
window.HouzyFlurik = function (host) {
  var W = 740, H = 500, cv = document.createElement('canvas');
  cv.width = W; cv.height = H; cv.className = 'pbg'; cv.setAttribute('aria-hidden', 'true');
  var gl = cv.getContext('webgl', { antialias: false, alpha: false, preserveDrawingBuffer: false });
  if (!gl) return null;
  var FS = "\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n\nuniform vec3 u_colors[8];\nuniform vec4 u_scene;      // resolution.xy, time, colour count\nuniform vec4 u_shape;      // scale, intensity, paramA, warp\nuniform vec4 u_surface;    // detail, contrast, brightness, saturation\nuniform vec4 u_finish;     // hue, vignette, blur, grain\nuniform vec4 u_transform;  // seed, rotation, drift, OKLab toggle\nuniform vec4 u_space;      // offset.xy, pointer.xy\nuniform vec4 u_cursor;\n\n#define u_resolution u_scene.xy\n#define u_time u_scene.z\n#define u_colorCount u_scene.w\n#define u_scale u_shape.x\n#define u_intensity u_shape.y\n#define u_paramA u_shape.z\n#define u_warp u_shape.w\n#define u_detail u_surface.x\n#define u_contrast u_surface.y\n#define u_brightness u_surface.z\n#define u_saturation u_surface.w\n#define u_hue u_finish.x\n#define u_vignette u_finish.y\n#define u_blur u_finish.z\n#define u_grain u_finish.w\n#ifdef GL_FRAGMENT_PRECISION_HIGH\n#define u_seed u_transform.x\n#else\n#define u_seed mod(u_transform.x, 31.0)\n#endif\n#define u_rotate u_transform.y\n#define u_drift u_transform.z\n#define u_oklab u_transform.w\n#define u_offset u_space.xy\n#define u_mouse u_space.zw\n#define u_cursorPresence u_cursor.x\n#define u_cursorEffect u_cursor.y\n#define u_cursorStrength u_cursor.z\n#define u_cursorRadius u_cursor.w\n\nfloat hash21(vec2 p) {\n#ifndef GL_FRAGMENT_PRECISION_HIGH\n  p = mod(p, 31.0);\n#endif\n  p = fract(p * vec2(234.34, 435.345));\n  p += dot(p, p + 34.23);\n  return fract(p.x * p.y);\n}\n\nfloat grainHash(vec2 p) {\n  vec3 p3 = fract(vec3(p.xyx) * 0.1031);\n  p3 += dot(p3, p3.yzx + 33.33);\n  return fract((p3.x + p3.y) * p3.z);\n}\n\nvec2 hash22(vec2 p) {\n#ifndef GL_FRAGMENT_PRECISION_HIGH\n  p = mod(p, 31.0);\n#endif\n  float n = sin(dot(p, vec2(41.0, 289.0)));\n  return fract(vec2(15731.743, 7892.321) * n);\n}\n\nfloat noise(vec2 p) {\n  vec2 i = floor(p);\n  vec2 f = fract(p);\n  vec2 u = f * f * (3.0 - 2.0 * f);\n  return mix(\n    mix(hash21(i), hash21(i + vec2(1.0, 0.0)), u.x),\n    mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), u.x),\n    u.y);\n}\n\nfloat fbm(vec2 p) {\n  float v = 0.0;\n  float a = 0.5;\n  for (int i = 0; i < 5; i++) {\n    v += a * noise(p);\n    p = p * 2.03 + vec2(17.0, 9.2);\n    a *= 0.5;\n  }\n  return v;\n}\n\nvec3 srgbToLinear(vec3 c) {\n  return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),\n    step(0.04045, c));\n}\nvec3 linearToSrgb(vec3 c) {\n  return mix(c * 12.92, 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,\n    step(0.0031308, c));\n}\nvec3 linToOklab(vec3 c) {\n  float l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;\n  float m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;\n  float s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;\n  l = pow(max(l, 0.0), 1.0 / 3.0);\n  m = pow(max(m, 0.0), 1.0 / 3.0);\n  s = pow(max(s, 0.0), 1.0 / 3.0);\n  return vec3(\n    0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,\n    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,\n    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s);\n}\nvec3 oklabToLin(vec3 c) {\n  float l = c.x + 0.3963377774 * c.y + 0.2158037573 * c.z;\n  float m = c.x - 0.1055613458 * c.y - 0.0638541728 * c.z;\n  float s = c.x - 0.0894841775 * c.y - 1.2914855480 * c.z;\n  l = l * l * l; m = m * m * m; s = s * s * s;\n  return vec3(\n    4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,\n    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,\n    -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s);\n}\nvec3 mixColour(vec3 a, vec3 b, float t) {\n  if (u_oklab > 0.5) {\n    vec3 la = linToOklab(srgbToLinear(a));\n    vec3 lb = linToOklab(srgbToLinear(b));\n    return clamp(linearToSrgb(oklabToLin(mix(la, lb, t))), 0.0, 1.0);\n  }\n  return mix(a, b, t);\n}\n\nvec3 palette(float x) {\n  float n = max(u_colorCount - 1.0, 1.0);\n  float f = clamp(x, 0.0, 1.0) * n;\n  vec3 col = u_colors[0];\n  for (int i = 0; i < 7; i++) {\n    if (float(i) < n)\n      col = mixColour(col, u_colors[i + 1],\n        smoothstep(0.0, 1.0, clamp(f - float(i), 0.0, 1.0)));\n  }\n  return col;\n}\n\nvec3 hueRotate(vec3 col, float a) {\n  const mat3 toYIQ = mat3(0.299, 0.596, 0.211,\n                          0.587, -0.274, -0.523,\n                          0.114, -0.322, 0.312);\n  const mat3 toRGB = mat3(1.0, 1.0, 1.0,\n                          0.956, -0.272, -1.106,\n                          0.621, -0.647, 1.703);\n  vec3 yiq = toYIQ * col;\n  float ca = cos(a), sa = sin(a);\n  yiq = vec3(yiq.x, yiq.y * ca - yiq.z * sa, yiq.y * sa + yiq.z * ca);\n  return toRGB * yiq;\n}\n\nvec3 shade(vec2 uv, vec2 p, float t) {\n  float y = uv.y\n    + sin(uv.x * (3.0 + u_intensity * 9.0) + t * 0.8) * 0.08\n    + (fbm(p * 2.0 + t * 0.1) - 0.5) * u_intensity * 0.6;\n  return palette(y);\n}\n\nvoid main() {\n  vec2 uv = gl_FragCoord.xy / u_resolution.xy;\n  vec2 screenUv = uv;\n  vec2 p = (gl_FragCoord.xy - 0.5 * u_resolution.xy)\n    / min(u_resolution.x, u_resolution.y);\n  float cursorMask = 0.0;\n\n  if (u_cursorPresence > 0.001) {\n    vec2 cursor = (0.5 * u_mouse * u_resolution.xy)\n      / min(u_resolution.x, u_resolution.y);\n    vec2 cursorDelta = p - cursor;\n    if (u_cursorEffect < 0.5) {\n      p += cursor * u_cursorPresence * u_cursorStrength * 0.55;\n    } else {\n      float cursorDistance = length(cursorDelta);\n      vec2 cursorDirection = cursorDelta / max(cursorDistance, 0.0001);\n      cursorMask = u_cursorPresence\n        * (1.0 - smoothstep(0.0, u_cursorRadius, cursorDistance));\n      if (u_cursorEffect < 1.5) {\n        p -= cursorDirection * cursorMask * u_cursorStrength * 0.24;\n      } else if (u_cursorEffect < 2.5) {\n        float cursorAngle = cursorMask * u_cursorStrength * 2.2;\n        float cc = cos(cursorAngle), cs = sin(cursorAngle);\n        p = cursor + mat2(cc, -cs, cs, cc) * cursorDelta;\n      } else if (u_cursorEffect < 3.5) {\n        float ripple = sin(\n          cursorDistance / max(u_cursorRadius, 0.001) * 18.0 - u_time * 5.0);\n        p -= cursorDirection * ripple * cursorMask * u_cursorStrength * 0.07;\n      }\n    }\n  }\n\n  uv = p * min(u_resolution.x, u_resolution.y) / u_resolution.xy + 0.5;\n  p *= u_scale;\n  if (abs(u_rotate) > 0.0001) {\n    float cr = cos(u_rotate), sr = sin(u_rotate);\n    p = mat2(cr, -sr, sr, cr) * p;\n  }\n  p += u_offset;\n  if (u_drift > 0.0001)\n    p += u_drift * vec2(sin(u_time * 0.31), cos(u_time * 0.23));\n  if (u_warp > 0.0) {\n    p += u_warp * (vec2(\n      fbm(p * u_detail + u_seed),\n      fbm(p * u_detail + vec2(5.2, 1.3))) - 0.5);\n  }\n  vec3 col;\n  if (u_blur > 0.0) {\n    float e = u_blur;\n    float pe = e * u_scale;\n    vec2 uvE = vec2(e) * min(u_resolution.x, u_resolution.y) / u_resolution.xy;\n    col  = shade(uv, p, u_time) * 0.36;\n    col += shade(uv + vec2(uvE.x, 0.0), p + vec2(pe, 0.0), u_time) * 0.16;\n    col += shade(uv - vec2(uvE.x, 0.0), p - vec2(pe, 0.0), u_time) * 0.16;\n    col += shade(uv + vec2(0.0, uvE.y), p + vec2(0.0, pe), u_time) * 0.16;\n    col += shade(uv - vec2(0.0, uvE.y), p - vec2(0.0, pe), u_time) * 0.16;\n  } else {\n    col = shade(uv, p, u_time);\n  }\n  if (abs(u_contrast - 1.0) > 0.0001)\n    col = (col - 0.5) * u_contrast + 0.5;\n  if (abs(u_saturation - 1.0) > 0.0001) {\n    float luma = dot(col, vec3(0.299, 0.587, 0.114));\n    col = mix(vec3(luma), col, u_saturation);\n  }\n  if (abs(u_hue) > 0.0001)\n    col = hueRotate(col, u_hue);\n  if (abs(u_brightness) > 0.0001)\n    col += u_brightness;\n  if (u_vignette > 0.0001) {\n    float vd = length(screenUv - 0.5) * 1.41421356;\n    col *= 1.0 - u_vignette * smoothstep(0.35, 1.0, vd);\n  }\n  if (u_cursorPresence > 0.001 && u_cursorEffect > 3.5)\n    col += (vec3(0.18) + col * 0.12) * cursorMask * u_cursorStrength;\n  if (u_grain > 0.0001)\n    col += (grainHash(\n      gl_FragCoord.xy + vec2(u_seed * 17.0, u_seed * 31.0)) - 0.5) * u_grain;\n  gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n}\n";
  var VS = 'attribute vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n';
  function sh(type, src) { var s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s); return gl.getShaderParameter(s, gl.COMPILE_STATUS) ? s : null; }
  var vs = sh(gl.VERTEX_SHADER, VS), fs = sh(gl.FRAGMENT_SHADER, FS);
  if (!vs || !fs) return null;
  var pr = gl.createProgram(); gl.attachShader(pr, vs); gl.attachShader(pr, fs); gl.linkProgram(pr);
  if (!gl.getProgramParameter(pr, gl.LINK_STATUS)) return null;
  gl.useProgram(pr);
  var buf = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  var loc = gl.getAttribLocation(pr, 'p'); gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  var U = function (n) { return gl.getUniformLocation(pr, n); };
  gl.uniform3fv(U('u_colors'), new Float32Array([0.024, 0.129, 0.169, 0.059, 0.486, 0.549, 0.310, 0.820, 0.773, 0.024, 0.129, 0.169,
                                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]));
  gl.uniform4f(U('u_shape'), 1.5, 0.5, 0.5, 0);
  gl.uniform4f(U('u_surface'), 2.4, 1, 0, 1);
  gl.uniform4f(U('u_finish'), 0, 0.25, 0, 0.03);
  gl.uniform4f(U('u_transform'), 3, 0, 0, 0);
  var uScene = U('u_scene'), uSpace = U('u_space'), uCursor = U('u_cursor');
  gl.viewport(0, 0, W, H);
  host.insertBefore(cv, host.firstChild);

  var mx = W / 2, my = H / 2, inside = 0, pres = 0, running = false, onScreen = true, last = 0;
  host.addEventListener('pointermove', function (e) {
    var r = host.getBoundingClientRect();
    mx = (e.clientX - r.left) / r.width * W; my = (e.clientY - r.top) / r.height * H; inside = 1;
  });
  host.addEventListener('pointerleave', function () { inside = 0; });
  if ('IntersectionObserver' in window) new IntersectionObserver(function (e) { onScreen = e[0].isIntersecting; }).observe(host);
  function frame(now) {
    if (!running) return;
    requestAnimationFrame(frame);
    if (!onScreen || document.hidden || now - last < 33) return;   // 30 fps, as the plugin's theme tick
    last = now;
    pres += (inside - pres) * 0.2;
    gl.uniform4f(uScene, W, H, (now / 1000 % 3600) * 0.37, 4);
    gl.uniform4f(uSpace, 0, 0, mx / W * 2 - 1, 1 - my / H * 2);
    gl.uniform4f(uCursor, pres, 4, 0.20, 0.39);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  return {
    show: function (on) {
      cv.style.display = on ? '' : 'none';
      if (on && !running) { running = true; requestAnimationFrame(frame); }
      if (!on) running = false;
    }
  };
};
