# Babuun Hollow 🐒 (VR para Oculus Go)

Jogo de terror em VR, **APK nativo (VrApi / `libvrapi.so`)** para o **Oculus Go**, instalável por sideload.
Você é um **macaco sem pernas** e anda só com as mãos (gorilla locomotion acelerada e mais forte).
Fique preso num depósito subterrâneo abandonado (concreto, ladrilhos, luzes fluorescentes, corredores
de prateleiras cheias de brinquedos), pegue as bananas douradas e fuja antes que os babuínos te peguem.

> Jogo original inspirado no gênero "macaco sem pernas + terror". Todos os modelos foram modelados via
> **Blender CLI**, todas as texturas são procedurais e todos os sons foram sintetizados do zero.

![preview](docs/preview.png)

## Instalar no Oculus Go (sideload)

1. Ative o **Modo Desenvolvedor** do Go (app Oculus no celular → Dispositivos → Modo do desenvolvedor).
2. Conecte o Go no PC e instale com **SideQuest** (arraste o APK) ou via adb:
   ```bash
   adb install -r dist/BabuunHollow.apk
   # opcional: já libera microfone e armazenamento (para gravar vídeos com voz)
   adb shell pm grant com.babuun.hollow android.permission.RECORD_AUDIO
   adb shell pm grant com.babuun.hollow android.permission.WRITE_EXTERNAL_STORAGE
   ```
3. No Go: **Biblioteca → Fontes desconhecidas → Babuun Hollow**.

Os vídeos gravados ficam em `/sdcard/Movies/BabuunHollow/` (ou em
`/sdcard/Android/data/com.babuun.hollow/files/Movies/` se a permissão de armazenamento for negada).

## Controles (um controle só)

| Ação | Como |
|---|---|
| Andar / correr | Aponte o controle para baixo até a mão tocar o chão e **empurre para trás**. Solte no meio do movimento para ganhar impulso. |
| Mão duplicada | A outra mão é controlada automaticamente (veja os modos abaixo) |
| Girar | Clique no touchpad **esquerda/direita** (30° ou 45°) |
| Grito de macaco | Clique no **centro** do touchpad (cuidado: monstros escutam!) |
| Pegar / guardar câmera | Botão **VOLTAR** (ou o botão "PEGAR CÂMERA" no lobby) |
| Gravar / parar | **Gatilho** (com a câmera na mão ou parada no ar) |
| Esticar o bastão | **Deslize** o dedo para cima/baixo no touchpad (0,12 m a 2,6 m) |
| Selfie ↔ mundo | Clique em **cima** no touchpad |
| Deixar câmera parada no ar | Clique em **baixo** no touchpad (VOLTAR pega de novo) |
| Botões do lobby | Toque com qualquer mão **ou** aponte o laser e aperte o gatilho |

### Mão duplicada (Go só tem 1 controle)
- **ESPELHO** (padrão): a mão duplicada copia a do controle espelhada. As duas empurram juntas, e por isso
  dá para **andar reto**. Quando a mão sobe, as duas se aproximam do centro (vão se juntando).
- **ALTERNADA**: enquanto a mão do controle empurra o chão, a duplicada vai para a **direção oposta**
  (para a frente e para cima). Quando a mão levanta, as duas se juntam de novo.
- Ajustes no lobby: **impulso** (Normal / Forte / Turbo), **ângulo do braço**, giro, volume.

## O jogo
- **Lobby (sala de controle)**:
  - **estação de cores** com botões R/G/B de 0 a 9 dentro do VR, 8 cores prontas e uma opção aleatória;
  - **guarda-roupa** com manequim (chapéus, óculos, gravata e cor do acessório);
  - **quadro de badges** (toque numa badge para usá-la como título acima do nome);
  - **multiplayer** (sala, conectar, novo nome), **ajustes**, **como jogar** e **suporte da câmera**.
- **O Depósito**: corredores de concreto, salas de máquinas, escritórios e corredores de prateleiras que
  dá para escalar. Pegue **8 bananas douradas** para abrir a **porta de aço** e fuja.
- **Monstros (com jumpscare cada um)**:
  - **Babuun**: patrulha, ouve barulho, persegue rápido e **salta** em quem está em cima das prateleiras.
  - **Pálido**: alto e esquelético; **só se move quando ninguém está olhando para ele**. Acorda com 2 bananas.
  - **Mamãe**: enorme; ouve qualquer barulho de longe, dá investidas e depois cansa. Acorda com 5 bananas.
- **14 badges**: Primeiro Passo, Banana!, Colecionador, Fuga!, Sobrevivente, Mordido, Não Pisque,
  Mamãe Brava, Macaco Social, Estilista, Cinegrafista, Alpinista, Velocista, Veterano.

## Multiplayer: servidor `us-lax01.altare.global:25570`
O jogo conecta por **TCP** em `us-lax01.altare.global:25570`. O servidor está em `server/server.js`
(Node.js ≥ 14, sem dependências):
```bash
node server/server.js            # porta 25570
PORT=25570 MAX_PER_ROOM=10 node server/server.js
```
- No painel da Altare, crie um servidor **Node.js** com arquivo de inicialização `server/server.js` na porta 25570.
- Abrir `http://us-lax01.altare.global:25570` no navegador mostra o status (salas e jogadores).
- O primeiro jogador da sala vira **host**: ele simula os monstros e a rodada. Se sair, outro assume.
- Para mudar o servidor, edite `SERVER_HOST`/`SERVER_PORT` em `app/jni/net.h` e recompile.

## Compilar
Requisitos: Android NDK r21e, build-tools 30 (aapt/zipalign/apksigner), `android.jar` (API 26), Java, Node, Blender 4.2.
```bash
node tools/genmap.mjs                         # mapa (shared/map.json + app/assets/map.txt)
python3 tools/gen_textures.py                 # texturas procedurais
python3 tools/gen_sounds.py                   # sons sintetizados
python3 tools/gen_font.py                     # fonte do texto em VR
blender -b -P tools/blender/build_assets.py   # TODOS os modelos 3D (personagens, monstros, mapa...)
./build.sh                                    # -> dist/BabuunHollow.apk
```
O `build.sh` baixa automaticamente o VrApi 1.14 (headers + `libvrapi.so` 32-bit para o Go) em `third_party/`.

Testes no PC (sem óculos):
```bash
g++ -std=c++14 -o build/world_test tools/test/world_test.cpp app/jni/world.cpp && ./build/world_test
# loco_test: simula a gorilla locomotion (os dois modos) e a IA dos três monstros
```
`python3 tools/preview_bm.py saida.png app/assets/models/babuun.bm` renderiza qualquer modelo `.bm`.

## Estrutura
```
app/jni/        motor C++ (VrApi, GLES3, OpenSL ES, MediaCodec)
  main.cpp        ciclo de vida, VrApi, controle do Go, câmera FBO/PBO, JNI
  game*.cpp       jogador, locomoção, rodada, monstros (BFS), lobby, HUD, jumpscare
  gfx.cpp         shaders (névoa, luzes fluorescentes, AO por vértice, vento), modelos .bm, texto
  audio.cpp       mixer 3D + microfone   recorder.cpp  vídeo H.264 + AAC -> MP4
  net.cpp         cliente TCP            world.cpp     colisão/raycast
app/assets/     modelos .bm, texturas .tex, sons .wav, mapa
tools/          gerador de mapa, texturas, sons, fonte, script do Blender, testes
server/         servidor multiplayer (Node)
dist/           APK pronto para sideload
```

## Licenças
`libvrapi.so` faz parte do Oculus Mobile SDK e é redistribuída dentro do APK conforme a
*Oculus SDK License* (veja `dist/licencas/`).
