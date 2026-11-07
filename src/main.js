// Babuun Hollow — WebXR para Oculus Go (navegador Oculus). Também roda no PC (modo tela) para testes.
import * as THREE from 'three';
import { loadModels, loadText } from './assets.js';
import { audio } from './audio.js';
import { World } from './world.js';
import { Game } from './game.js';
import { askMic } from './camera.js';
import WebXRPolyfill from 'webxr-polyfill';

// navegador só com WebVR (Oculus Go antigo): traduz para WebXR
if (!navigator.xr) { try { new WebXRPolyfill({ cardboard: false }); } catch (e) { console.warn(e); } }

const MODELS = ['map', 'gate', 'monkey', 'hand', 'arm', 'banana', 'babuun', 'palido', 'mamae', 'camera', 'stick', 'grip', 'button', 'board',
  'cos_cap', 'cos_beanie', 'cos_tophat', 'cos_crown', 'cos_leafcrown', 'cos_horns', 'cos_headphones', 'cos_glasses', 'cos_shades', 'cos_bowtie'];
const SOUNDS = ['tap_wood', 'tap_ground', 'tap_stone', 'tap_metal', 'tap_leaf', 'whoosh', 'banana', 'badge', 'ui_click', 'ui_error', 'rec_start', 'rec_stop',
  'gate_open', 'hoot', 'babuun_growl', 'babuun_bark', 'babuun_screech', 'palido_breath', 'palido_shriek', 'mamae_roar', 'step_light', 'step_heavy',
  'js_babuun', 'js_palido', 'js_mamae', 'amb_night', 'amb_lobby', 'chase', 'heartbeat', 'escape'];

const $ = id => document.getElementById(id);
const status = t => { $('status').textContent = t; };

async function start() {
  // ---- renderizador
  const renderer = new THREE.WebGLRenderer({ antialias: false, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5));
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.xr.enabled = true;
  renderer.xr.setReferenceSpaceType('local');
  renderer.xr.setFoveation(1);
  $('view').appendChild(renderer.domElement);
  const scene = new THREE.Scene();
  scene.fog = new THREE.FogExp2(0x0d0d10, 0.05);
  const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.05, 120);
  const rig = new THREE.Group();
  rig.add(camera); scene.add(rig);
  addEventListener('resize', () => { camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); });

  // ---- carregamento
  status('Carregando mapa...');
  const world = new World(await loadText('map.txt'));
  await loadModels(MODELS, p => status(`Carregando modelos ${Math.round(p * 100)}%`));
  status('Carregando sons...');
  await audio.init(SOUNDS);
  const game = new Game(scene, rig, camera, world);
  window.game = game;
  status('Pronto!');
  $('enter').disabled = false;
  $('enter').textContent = navigator.xr ? 'ENTRAR NO VR' : 'ENTRAR NO VR (sem WebXR neste navegador)';
  $('desk').disabled = false;

  // ---- entrar no VR
  let session = null;
  $('enter').onclick = async () => {
    audio.resume();
    if (!navigator.xr) { status('Este navegador não tem WebXR. Abra no navegador do Oculus Go.'); return; }
    try {
      session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
      await renderer.xr.setSession(session);
      $('ui').style.display = 'none';
      session.addEventListener('end', () => { $('ui').style.display = ''; session = null; game.onAway(); });
      session.addEventListener('visibilitychange', () => { if (session.visibilityState !== 'visible') game.onAway(); });
    } catch (e) { status('Não foi possível entrar no VR: ' + e.message); }
  };
  $('mic').onclick = async () => { $('mic').textContent = (await askMic()) ? 'Microfone ativado ✔' : 'Microfone negado'; };
  $('desk').onclick = () => { audio.resume(); $('ui').style.display = 'none'; desktop.on = true; renderer.domElement.focus(); };
  document.addEventListener('visibilitychange', () => { if (document.hidden) game.onAway(); });
  // fechando a página: se for host, avisa e passa a sala para o próximo
  addEventListener('pagehide', () => { if (game.net.mode !== 'off') game.net.leave(true); });

  // ---- modo tela (PC): mouse olha, altura do mouse = inclinação do braço
  const desktop = { on: false, yaw: 0, pitch: 0, ctrlPitch: -0.6, keys: {}, drag: false, wheel: 0 };
  const cv = renderer.domElement;
  cv.addEventListener('mousedown', e => { if (e.button === 2) desktop.drag = true; else desktop.keys.trigger = true; });
  addEventListener('mouseup', e => { if (e.button === 2) desktop.drag = false; else desktop.keys.trigger = false; });
  cv.addEventListener('contextmenu', e => e.preventDefault());
  addEventListener('mousemove', e => {
    if (desktop.drag) { desktop.yaw -= e.movementX * 0.004; desktop.pitch = Math.max(-1.3, Math.min(1.3, desktop.pitch - e.movementY * 0.004)); }
    desktop.ctrlPitch = 0.5 - (e.clientY / innerHeight) * 2.0;
    desktop.ctrlYaw = (0.5 - e.clientX / innerWidth) * 1.2;
  });
  addEventListener('wheel', e => { desktop.wheel += -e.deltaY * 0.001; });
  addEventListener('keydown', e => { desktop.keys[e.code] = true; if (e.code === 'Escape') { $('ui').style.display = ''; desktop.on = false; } });
  addEventListener('keyup', e => { desktop.keys[e.code] = false; });
  $('ui').addEventListener('click', () => audio.resume());

  // ---- entrada do controle do Go (WebXR)
  const prev = { trig: false, click: false };
  const qTmp = new THREE.Quaternion();
  function readInput(frame) {
    const inp = { hasCtrl: false, ctrl: new THREE.Quaternion(), head: new THREE.Quaternion(), headPos: new THREE.Vector3(),
                  trigger: false, triggerDown: false, padClick: false, padClickDown: false, padTouch: false, padX: 0, padY: 0, leftHanded: false, wheel: 0 };
    if (frame && session) {
      const ref = renderer.xr.getReferenceSpace();
      const vp = frame.getViewerPose(ref);
      if (vp) {
        const o = vp.transform.orientation, p = vp.transform.position;
        inp.head.set(o.x, o.y, o.z, o.w); inp.headPos.set(p.x, p.y, p.z);
      }
      for (const src of session.inputSources) {
        if (!src.targetRaySpace || src.handedness === 'none' && session.inputSources.length > 1) continue;
        const pose = frame.getPose(src.targetRaySpace, ref);
        if (pose) { const o = pose.transform.orientation; inp.ctrl.set(o.x, o.y, o.z, o.w); inp.hasCtrl = true; }
        inp.leftHanded = src.handedness === 'left';
        const gp = src.gamepad;
        if (gp) {
          inp.trigger = !!(gp.buttons[0] && gp.buttons[0].pressed);
          for (let i = 1; i < gp.buttons.length; i++) { if (gp.buttons[i].pressed) inp.padClick = true; if (gp.buttons[i].touched) inp.padTouch = true; }
          // touchpad: usa o par de eixos com maior valor
          let best = 0;
          for (let i = 0; i + 1 < gp.axes.length; i += 2) {
            const m = Math.abs(gp.axes[i]) + Math.abs(gp.axes[i + 1]);
            if (m > best) { best = m; inp.padX = gp.axes[i]; inp.padY = -gp.axes[i + 1]; }
          }
          if (best > 0.05) inp.padTouch = true;
        }
        break;
      }
      if (!inp.hasCtrl) inp.ctrl.copy(inp.head);  // sem controle: usa a cabeça
    } else if (desktop.on) {
      const k = desktop.keys;
      inp.head.setFromEuler(new THREE.Euler(desktop.pitch, desktop.yaw, 0, 'YXZ'));
      inp.headPos.set(0, 0, 0);
      inp.ctrl.setFromEuler(new THREE.Euler(desktop.ctrlPitch, desktop.yaw + (desktop.ctrlYaw || 0), 0, 'YXZ'));
      inp.hasCtrl = true;
      inp.trigger = !!(k.trigger || k.Space);
      const L = k.KeyA || k.ArrowLeft, R = k.KeyD || k.ArrowRight, U = k.KeyC, D = k.KeyV, H = k.KeyQ;
      inp.padClick = !!(L || R || U || D || H);
      inp.padX = L ? -1 : R ? 1 : 0; inp.padY = U ? 1 : D ? -1 : 0;
      inp.wheel = desktop.wheel; desktop.wheel = 0;
    }
    inp.triggerDown = inp.trigger && !prev.trig; prev.trig = inp.trigger;
    inp.padClickDown = inp.padClick && !prev.click; prev.click = inp.padClick;
    return inp;
  }

  // ---- laço
  let last = performance.now();
  renderer.setAnimationLoop((t, frame) => {
    const dt = Math.min(0.05, (t - last) / 1000); last = t;
    const inp = readInput(frame);
    game.update(dt, inp);
    if (!session) { camera.position.set(0, 0, 0); camera.quaternion.copy(inp.head); }
    game.cam.render(renderer, scene, t / 1000);
    renderer.render(scene, camera);
  });
}

start().catch(e => { console.error(e); status('Erro: ' + e.message); });
