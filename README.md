# Downfall

Mini-engine e protótipo de jogo de tiro tático cooperativo em primeira pessoa (FPS) para até **4 jogadores**, desenvolvido em **C++17** e **Raylib** no Linux, sem dependências de engines ou bibliotecas externas.

O projeto combina a **dinâmica cooperativa de esquadrão** inspirada em *Army of Two* (troca ativa de *Aggro*, fogo de supressão bilateral, flanqueamento furtivo com dano bônus, pings contextuais e arrasto/reanimação de aliados feridos) com a **física vetorial de movimentação** da *Source Engine* (*Auto-Bunny Hop*, *Air Strafe*, corrida com FOV dinâmico e *Crouch-Jump*).

---

## Sumário

1. [Stack Tecnológica & Compilação](#1-stack-tecnológica--compilação)
2. [Arquitetura Modular](#2-arquitetura-modular)
3. [Mecânicas de Jogabilidade](#3-mecânicas-de-jogabilidade)
4. [Arsenal, Balística & Wallbang](#4-arsenal-balística--wallbang)
5. [Multiplayer Cooperativo LAN (UDP)](#5-multiplayer-cooperativo-lan-udp)
6. [Pipeline de Assets, Áudio 3D & Editor de Mapas](#6-pipeline-de-assets-áudio-3d--editor-de-mapas)
7. [Guia de Controles](#7-guia-de-controles)
8. [Histórico de Savepoints (01–19)](#8-histórico-de-savepoints-0119)

---

## 1. Stack Tecnológica & Compilação

* **Linguagem:** C++17 (STL padrão + sockets POSIX/Linux nativos).
* **Biblioteca Gráfica e Matemática:** Raylib (`raylib.h`, `raymath.h`, `rlgl.h`).
* **Sistema Operacional:** Linux.

### Compilando via Makefile
```bash
make run
```

### Compilação Direta via `g++`
```bash
g++ -std=c++17 -Wall -O2 main.cpp -o downfall -I/home/jose/raylib/src -L/home/jose/raylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 && ./downfall
```

---

## 2. Arquitetura Modular

O projeto está estruturado em 13 módulos de cabeçalho (`.hpp`) desacoplados, coordenados pelo loop principal em `main.cpp`.

```text
projeto/
├── main.cpp             # Loop principal, máquina de estados, HUD 2D e sincronização
├── Makefile             # Automação de build C++17 + Raylib
├── World.hpp            # Geometria AABB, raycast, presets e carregador de mapas
├── Player.hpp           # Física Source Engine, posturas e estado caído (Downed)
├── Weapon.hpp           # Arsenal (slots 1–5), viewmodel 3D, balística, wallbang e granadas
├── SquadAI.hpp          # IA de aliados e inimigos, aggro, cobertura, supressão e pings
├── Objectives.hpp       # Loop de missão, terminais, barris explosivos, caixas e extração
├── Cutscene.hpp         # Trilhos de câmera suaves, letterbox, legendas e trigger zones
├── Audio.hpp            # Áudio espacial 3D, trilha dinâmica e sintetizador PCM procedural
├── AssetManager.hpp     # Carregamento híbrido de modelos .glb (Blender) com fallback
├── MapEditor.hpp        # Editor de níveis 3D in-game com exportação para .txt
├── SaveSystem.hpp       # Persistência de loadout, configurações e progresso (savegame.cfg)
├── Network.hpp          # Multiplayer LAN via sockets UDP não-bloqueantes (Host/Cliente)
├── Menu.hpp             # Interface de Menu Principal, Briefing/Loadout, Lobby LAN e Opções
└── assets/              # Pastas opcionais para modelos (.glb), efeitos (.wav/.ogg) e músicas
```

### Responsabilidade de Cada Módulo

| Arquivo | Estruturas / Classes Principais | Função no Sistema |
| :--- | :--- | :--- |
| **`World.hpp`** | `WorldBlock`, `MapPreset`, `LevelLoader` | Lê e grava cenários em `.txt` (`posX posY posZ tamX tamY tamZ R G B`), gera os mapas padrão (`mapa_squad.txt`, `mapa_movimento.txt`, `mapa_treino.txt`) e executa testes de linha de visão (`HasLineOfSight`). |
| **`Player.hpp`** | `SourcePlayer` | Implementa aceleração no solo/ar, atrito, *Auto-Bhop*, *Air Strafe*, sprint, agachamento com checagem de teto, *Crouch-Jump* e colisão AABB estrita. |
| **`Weapon.hpp`** | `WeaponId`, `WeaponStats`, `ViewmodelWeapon`, `ThrownGrenade` | Controla os 8 equipamentos (slots 1 a 5), recuo, *sway*, renderização *viewmodel* (`rlgl.h`), penetração balística (*wallbang*) e rebote físico de granadas. |
| **`SquadAI.hpp`** | `SquadAlly`, `EnemyBot`, `SquadOrder`, `TacticalPing`, `DamageIndicator` | Gerencia seleção de alvos por *Aggro*, busca de cobertura (`FindBestCoverPoint`), supressão bilateral, hitboxes separadas (cabeça/tronco) e pings contextuais. |
| **`Objectives.hpp`** | `ExplosiveBarrel`, `AmmoCrate`, `HackTerminal`, `ExtractionZone`, `MissionManager` | Controla as etapas da missão (`HACK_TERMINALS`, `CLEAR_HOSTILES`, `EXTRACT_SQUAD`), explosões em cadeia e tela de resultados. |
| **`Cutscene.hpp`** | `CutsceneKeyframe`, `CutsceneSequence`, `TriggerZone`, `CutsceneManager` | Interpola câmera (posição, alvo e FOV) com curva `SmootherStep`, anima barras *letterbox*, exibe legendas sincronizadas e monitora volumes de gatilho 3D. |
| **`Audio.hpp`** | `SfxId`, `ProceduralAudio` | Sintetiza ondas PCM 16-bit/44.1 kHz em memória ou carrega `.ogg`/`.wav` externos, aplica atenuação e *panning* estéreo 3D e mixa a trilha dinâmica. |
| **`AssetManager.hpp`** | `AssetManager` | Carrega modelos `.glb` e animações esqueléticas do Blender com *hot-reload* (`F7`) e *fallback* automático para geometria procedural. |
| **`MapEditor.hpp`** | `InGameMapEditor` | Permite criar, mover, redimensionar, colorir, desfazer (`Z`) e salvar blocos 3D em tempo real usando câmera livre (*noclip*). |
| **`SaveSystem.hpp`** | `PlayerSaveData`, `SaveSystem` | Salva e carrega loadout, sensibilidade, FOV, volumes de áudio, fases desbloqueadas e estatísticas acumuladas em `savegame.cfg`. |
| **`Network.hpp`** | `LanNetworkManager`, `NetClientInputPacket`, `NetWorldSnapshotPacket` | Gerencia sockets UDP não-bloqueantes na porta `27015` com arquitetura **Host-Autoritativo** e substituição dinâmica entre jogadores e bots. |
| **`Menu.hpp`** | `GameMode`, `MenuScreen`, `MainMenu` | Implementa as telas de Menu Principal, Briefing & Loadout, Lobby Multiplayer LAN e Configurações com sliders interativos. |

---

## 3. Mecânicas de Jogabilidade

### 3.1. Física de Movimentação (`Player.hpp`)
* **Resolução de Colisão AABB:** Resolvida separadamente na ordem estrita **`X` $\rightarrow$ `Z` $\rightarrow$ `Y`** com desigualdades estritas (`CheckOverlapStrict`), garantindo deslize suave ao encostar em paredes e quinas.
* **Constantes Físicas Principais:**
  * `AIR_SPEED_CAP = 0.75f`: Limite de projeção vetorial no ar que permite *Air Strafe* e ganho contínuo de velocidade com *Auto-Bhop* (segurando `Espaço`).
  * `MAX_SPRINT_SPD = 9.8f`: Velocidade máxima em corrida (`Left Shift`), expandindo o FOV dinamicamente em $+8^\circ$ sobre o FOV base configurado.
  * `CROUCH_HEIGHT = 1.00f` e `HEIGHT_DIFF = 0.80f`: Agachar no chão verifica colisão com o teto antes de levantar (`OverlapsAnyBlock`); agachar no ar executa o **Crouch-Jump**, elevando a base dos pés enquanto mantém a câmera estável.
  * `MAX_DRAG_SPD = 3.4f` / `MAX_DOWNED_SPD = 1.6f`: Limites de velocidade ao arrastar um aliado ferido ou rastejar no estado caído.

### 3.2. Troca de Aggro & Flanqueamento Furtivo (`SquadAI.hpp`)
* Cada membro do esquadrão (**Alpha**, **Bravo**, **Charlie**, **Delta**) possui um medidor de ameaça (`aggro` de $0$ a $100$).
* Os inimigos miram no membro do esquadrão com maior ameaça efetiva (multiplicada por $0.4$ quando não há linha de visão direta).
* Atirar aumenta o `aggro` do atirador ($+12.0$ por disparo, $+18.0$ por acerto), enquanto permanecer atrás de cobertura acelera o decaimento ($15.0/\text{s}$ oculto contra $6.5/\text{s}$ exposto).
* **Modo Furtivo (`isStealthFlank`):** Ativado automaticamente quando o jogador mantém `aggro < 25.0` enquanto pelo menos um aliado sustenta `aggro > 50.0`. O indicador da mira e o LED da arma mudam para azul (`SKYBLUE`) e todos os disparos do jogador causam **$2\times$ de dano**.

### 3.3. Supressão Bilateral, Cobertura & Ordens (`SquadAI.hpp`)
* Disparos próximos ou impactos elevam a barra de `suppression` ($0$ a $100$) de aliados e inimigos.
* **Histerese de Supressão:** Ao atingir `suppression >= 45.0`, o combatente agacha sua hitbox de $1.90\text{ m}$ para $1.25\text{ m}$, interrompe disparos e busca o ponto protegido mais próximo atrás de blocos sólidos (`FindBestCoverPoint`). O retorno ao combate ocorre apenas quando `suppression < 20.0`.
* **Ordens Globais (`F1`–`F3`):**
  * **`[F1]` Avançar:** Avança o esquadrão ofensivamente pelo mapa.
  * **`[F2]` Manter Posição:** Ancora os aliados em suas posições atuais (`anchorPos`) com marcação circular no solo.
  * **`[F3]` Fogo de Supressão:** Eleva o piso de *Aggro* dos aliados para $65.0$ e aumenta a cadência de tiro para atrair o fogo inimigo e abrir flancos.
* **Ping Tático Contextual (`Scroll Click`):**
  * Apontar para um bloco do cenário calcula o lado protegido contra a ameaça ativa e envia apenas o aliado mais próximo para se abrigar ali.
  * Apontar para um inimigo marca-o como `[ALVO PRIORITARIO]` para foco de fogo do esquadrão.

### 3.4. Estado Caído (*Downed*), Arrasto & Reanimação
* Ao ter o HP reduzido a zero, o operador entra no estado `DOWNED` com temporizador de sangramento de $25\text{ s}$ e ameaça zerada.
* Aliados saudáveis deslocam-se automaticamente para reanimar o jogador caso ele caia.
* O jogador pode segurar **`[E]`** para arrastar um aliado caído para trás de cobertura (mantendo a capacidade de atirar com a arma equipada) e segurar **`[F]`** para concluir a reanimação.

### 3.5. Loop de Missão & Progressão (`Objectives.hpp` & `SaveSystem.hpp`)
No modo **Missão de Esquadrão**, a partida se desenvolve em três etapas:
1. **`HACK_TERMINALS`:** Infiltrar-se e hackear os terminais criptografados (`[F]`).
2. **`CLEAR_HOSTILES`:** Eliminar os reforços inimigos acionados pelo alarme.
3. **`EXTRACT_SQUAD`:** Reunir os membros vivos do esquadrão no perímetro verde de extração.

Vencer uma missão desbloqueia a próxima fase (Fases 1 a 3, com aumento progressivo de reforços e resistência inimiga) e registra as estatísticas acumuladas em **`savegame.cfg`**.

---

## 4. Arsenal, Balística & Wallbang

| Slot | Equipamento | Características | Penetração (*Wallbang*) |
| :---: | :--- | :--- | :---: |
| **`1`** | **Fuzil de Assalto 5.56** | Automático, equilibrado para média distância | $1.05\text{ m}$ |
| **`1`** | **Escopeta Tática 12 GA** | Dispara 8 balotes simultâneos com alto dano próximo | — |
| **`1`** | **Metralhadora Leve (LMG)** | Alta cadência, supressão massiva e máxima penetração | $1.65\text{ m}$ |
| **`2`** | **Pistola Tática 9mm** | Saque rápido, carregador de 15 munições | — |
| **`2`** | **Revólver .44 Magnum** | Alto dano por disparo e perfuração de coberturas leves | $1.35\text{ m}$ |
| **`3`** | **Faca Tática** | Ataque corpo a corpo silencioso (não gera *Aggro* de disparo) | — |
| **`4`** | **Granada de Fragmentação** | Rebote físico AABB e detonação explosiva em área ($115$ de dano) | Área |
| **`5`** | **Granada Flashbang** | Cega e eleva a supressão dos inimigos no raio para $100\%$ | Área |

* Pressionar **`1`** ou **`2`** novamente alterna entre as sub-armas disponíveis naquele slot.
* **Headshots & Wallbang:** Acertos na caixa de colisão da cabeça (`GetHeadAABB`) aplicam **$2.5\times$ de dano**. Disparos que atravessam obstáculos dentro do limite de penetração sofrem atenuação proporcional à espessura atravessada e geram faíscas na face de saída.

---

## 5. Multiplayer Cooperativo LAN (UDP)

O modo cooperativo utiliza **sockets UDP não-bloqueantes** nativos do Linux na porta **`27015`** (`Network.hpp`):

* **Arquitetura Host-Autoritativo:**
  * O jogador que cria a sala (**Host**) assume o líder **Alpha (Slot 0)**, executa a simulação de IA, balística, barris explosivos e objetivos, e transmite pacotes `NetWorldSnapshotPacket` a $30\text{ Hz}$.
  * Até 3 jogadores (**Clientes**) conectam-se informando o IPv4 do Host (detectado automaticamente na tela do lobby ou via `127.0.0.1` para testes locais) e assumem os slots **Bravo (1)**, **Charlie (2)** e **Delta (3)**.
  * Clientes enviam pacotes `NetClientInputPacket` a $45\text{ Hz}$ (e instantaneamente em disparos, granadas ou pings).
* **Drop-In & Fallback de IA:** Jogadores podem entrar no lobby antes da partida ou assumir vagas livres durante a missão. Se um jogador desconectar ou atingir o *timeout* de $6.0\text{ s}$, o bot de IA reassume o personagem automaticamente.

---

## 6. Pipeline de Assets, Áudio 3D & Editor de Mapas

### Modelos 3D do Blender (`AssetManager.hpp`)
Coloque arquivos `.glb` (com suporte opcional a animações esqueléticas) na pasta `assets/models/` e pressione **`[F7]`** a qualquer momento para recarregá-los em tempo real (*Hot-Reload*):
* **Personagens:** `ally.glb`, `enemy.glb`
* **Armas e Gadgets:** `weapon_rifle.glb`, `weapon_shotgun.glb`, `weapon_lmg.glb`, `weapon_pistol.glb`, `weapon_revolver.glb`, `weapon_knife.glb`, `weapon_frag.glb`, `weapon_flash.glb`
* **Objetos:** `barrel.glb`, `crate.glb`, `terminal.glb`

*(Caso os arquivos `.glb` não estejam presentes, o motor utiliza automaticamente a renderização geométrica procedural).*

### Áudio Espacial 3D (`Audio.hpp`)
* Carrega arquivos `.ogg` ou `.wav` de `assets/sfx/` e `assets/music/` com *fallback* automático para efeitos sintetizados em PCM 16-bit/44.1 kHz em memória.
* Aplica atenuação por distância, *panning* estéreo baseado na orientação do ouvinte e *crossfade* contínuo entre trilha ambiente e combate conforme a tensão da partida.

### Editor de Mapas In-Game (`MapEditor.hpp`)
Acessível pelo menu principal, oferece câmera livre (*noclip*), grade magnética (`G`), modos `BUILD` e `EDIT` (`TAB`), redimensionamento (`J/L`, `U/O`, `I/K`), paleta de 8 cores (`1`–`8`), histórico de desfazer (`Z`) e exportação direta para `.txt` (`F5`).

---

## 7. Guia de Controles

### Em Partida (Combate & Movimentação)
| Tecla / Botão | Ação |
| :--- | :--- |
| **`W A S D`** | Movimentação vetorial (suporta *Air Strafe* no ar) |
| **`Espaço`** | Pular (segurar executa *Auto-Bunny Hop*; pula cenas em cutscenes) |
| **`Left Shift`** | Correr (*Sprint* com expansão dinâmica de FOV) |
| **`Left Ctrl`** | Agachar (no ar executa *Crouch-Jump*) |
| **`Mouse Esq.`** | Disparar arma / Arremessar granada |
| **`Scroll / 1–5`** | Selecionar slot (apertar `1` ou `2` novamente alterna sub-arma) |
| **`R`** | Recarregar arma atual |
| **`Botão Meio (Scroll)`** | Ping Tático Contextual (cobertura inteligente ou foco de fogo) |
| **`F1` / `F2` / `F3`** | Ordens ao Esquadrão: Avançar / Manter Posição / Fogo de Supressão |
| **`E` (Segurar)** | Arrastar aliado caído para cobertura (permite atirar enquanto arrasta) |
| **`F` (Segurar)** | Reanimar aliado caído, hackear terminal ou reabastecer na caixa |

### Ferramentas, Depuração & Editor
| Tecla | Contexto | Ação |
| :--- | :--- | :--- |
| **`ESC`** | Global | Voltar ao Menu Principal / Sair do sub-menu / Desconectar da sala LAN |
| **`M`** | Global | Alternar mudo do áudio |
| **`F5`** | Partida / Editor | Reiniciar cenário atual (Partida) / Salvar mapa `.txt` (Editor) |
| **`F6`** | Partida Solo | Disparar cutscene de demonstração manualmente |
| **`F7`** | Global | *Hot-Reload* de modelos `.glb` e áudios `.ogg`/`.wav` externos |
| **`F8`** | Partida | Alternar visualização 3D das *Trigger Zones* |
| **`TAB`** | Editor de Mapas | Alternar entre modo `BUILD` e `EDIT` |
| **`Setas` + `PgUp/PgDn`** | Editor de Mapas | Mover bloco selecionado pela grade magnética |
| **`J/L`, `U/O`, `I/K`** | Editor de Mapas | Redimensionar bloco nos eixos X, Y e Z |
| **`1–8` / `V` / `Z`** | Editor de Mapas | Selecionar cor / Duplicar bloco / Desfazer ação |

---

## 8. Histórico de Savepoints (01–19)

* **`SAVEPOINT_01`:** Física base *Source Engine* (`SourcePlayer`), carregador de mapas `.txt` e colisão AABB estrita (`X` $\rightarrow$ `Z` $\rightarrow$ `Y`).
* **`SAVEPOINT_02`:** Corrida com FOV dinâmico, agachamento com checagem de teto, *Crouch-Jump* e *viewmodel* 3D (`rlgl.h`).
* **`SAVEPOINT_03` / `03.5`:** Esquadrão de 4 operadores, sistema de *Aggro*, bônus furtivo ($2\times$) e refatoração modular inicial.
* **`SAVEPOINT_04`:** Estado caído (*Downed*), sangramento, arrasto (`E`) e reanimação cooperativa (`F`).
* **`SAVEPOINT_05`:** Ordens táticas globais (`ADVANCE`, `HOLD`, `SUPPRESS`) com âncoras visuais no solo.
* **`SAVEPOINT_06` / `07`:** Supressão bilateral, histerese de cobertura dinâmica (`FindBestCoverPoint`) e barras de status no HUD.
* **`SAVEPOINT_08`:** Menu principal e ambientes dedicados (Missão, Estande de Supressão e Pista de Movimentação).
* **`SAVEPOINT_09`:** Sintetizador de áudio PCM procedural em memória (`Audio.hpp`).
* **`SAVEPOINT_10`:** Arsenal completo de 8 equipamentos em 5 slots, granadas físicas com rebote e binds `F1`–`F3`.
* **`SAVEPOINT_11`:** Ping tático contextual no botão do meio do mouse (cobertura inteligente e foco de fogo).
* **`SAVEPOINT_12`:** Hitboxes separadas (cabeça $2.5\times$ / tronco), perfuração balística (*wallbang*), hitmarkers coloridos e indicador direcional de dano.
* **`SAVEPOINT_13`:** Loop completo de missão (terminais, reforços, extração), barris explosivos e caixas de suprimento (`Objectives.hpp`).
* **`SAVEPOINT_14`:** Editor de mapas 3D in-game com grade magnética e exportação para `.txt` (`MapEditor.hpp`).
* **`SAVEPOINT_15`:** Suporte híbrido a modelos `.glb` e animações esqueléticas do Blender com *hot-reload* (`AssetManager.hpp`).
* **`SAVEPOINT_16`:** Áudio espacial 3D com *panning* estéreo, suporte a `.ogg`/`.wav` e trilha sonora dinâmica (`Audio.hpp`).
* **`SAVEPOINT_17`:** Cutscenes com trilhos `SmootherStep`, *letterbox*, legendas e *Trigger Zones* (`Cutscene.hpp`).
* **`SAVEPOINT_18`:** Tela de Briefing/Loadout, sliders de configuração e persistência em `savegame.cfg` (`SaveSystem.hpp`).
* **`SAVEPOINT_19`:** Multiplayer cooperativo LAN via sockets UDP não-bloqueantes com arquitetura Host-Autoritativo (`Network.hpp`).