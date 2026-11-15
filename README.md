# Retro8 Voxel 3D (core para RetroArch)

Core "wrapper": carrega o `retro8_libretro_android.so` original (PICO-8), mantém jogo, áudio, input e save states
intactos, e troca a tela 2D por uma **carcaça de videogame 3D** com a tela embutida, onde **cada pixel do jogo vira uma coluna voxel**.
Render 100% software (sem OpenGL), saída 640x480.

## Instalar
1. Compile `retro8_voxel_libretro_android.so` (opções abaixo).
2. Copie para a pasta `cores` do RetroArch, **ao lado** do `retro8_libretro_android.so` original (não renomeie o original).
3. Abra uma ROM `.p8` / `.p8.png` com "Retro8 Voxel 3D".

## Compilar (arm64)
- **Termux no celular:** `sh build_termux.sh`
- **NDK:** `NDK=/caminho/ndk ./build_android.sh`
- **GitHub Actions:** veja a seção abaixo.

## Compilar no GitHub (GitHub Actions)
O arquivo `.github/build-android.yml` é o workflow de build (usa o NDK do runner, compila
`retro8_voxel.c` e sobe `retro8_voxel_libretro_android.so` como artefato).

**Para ativar neste repositório:** mova/renomeie o arquivo para
`.github/workflows/build-android.yml` (é só criar a pasta `workflows` e mover o arquivo).
Depois é só dar um push — o build roda sozinho e o `.so` aparece em
*Actions → build-android → Artifacts → retro8_voxel_libretro_android*.

> Por que o workflow não está em `.github/workflows/`? A ferramenta de GitHub usada para publicar
> este repositório bloqueia escrita nesse caminho (proteção contra CI malicioso), então o arquivo
> foi deixado ao lado, pronto para ser movido com um clique no GitHub.

## Controles extras
Analógico direito = gira a câmera · L3 = alterna voxel/plano · R3 = reseta a câmera.
Os botões do D-pad, A/B, Start/Select e L/R afundam na carcaça quando você aperta.

## Como funciona
- Heurística de altura: a cor mais comum do quadro vira o "piso"; as demais sobem conforme o brilho (2 a 8,5 unidades).
- Alturas têm suavização entre quadros (efeito "pop").
- Detalhes ajustáveis no topo de `retro8_voxel.c` (`SCREEN`, `ZF`, `CAMD`, `FOCAL`) e em `draw_shell()` (cores/layout da carcaça).

## Teste no PC
`sh test/run_host_test.sh [quadros] [stick_x] [stick_y] [botoes]` usa um core PICO-8 falso e gera `out.ppm`.
As imagens de preview (`preview_padrao.png` e `preview_camera_girada.png`) vêm no `.zip` original do projeto:
a publicação via GitHub não conseguiu subi-las porque o conector envia só texto (PNG seria corrompido).

## Limitações conhecidas
- Compilado/testado aqui só em x86 com core falso; o `.so` arm64 é compilado pelo CI (GitHub Actions) ou localmente via NDK/Termux.
- Não testado com o retro8 real em um aparelho. Se o retro8 usar um formato de pixel diferente de XRGB8888/RGB565/0RGB1555, as cores ficariam erradas.
