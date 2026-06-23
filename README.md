# Tomagoshi ESP32

Projeto Tamagoshi para placas ESP32: firmware para display ST7735 128x160, controles por botões e buzzer, mais arquivos de modelo 3D e G-code para impressão.

**Descrição**

- Este repositório contém o código-fonte de uma versão de um Tamagotchi ("Tomagoshi") para ESP32, artefatos 3D para impressão e um G-code pré-renderizado.
- O firmware exibe um personagem no display ST7735, mantém estados (energia, felicidade, fome), salva dados na EEPROM e inclui um mini-jogo.

**Estrutura do repositório**

- `v1_tomagoshi_code/`: código-fonte Arduino/ESP32 (arquivo principal: `v1_tomagoshi.ino`).
- `tomagoshi_3D_layout/`: modelos 3D para impressão (arquivos .3mf).

**Arquivo principal**

- [v1_tomagoshi_code/v1_tomagoshi.ino](v1_tomagoshi_code/v1_tomagoshi.ino): firmware que implementa a lógica do Tamagotchi, menu, jogo e persistência em EEPROM.

**Dependências de software**

- Biblioteca Adafruit_GFX
- Biblioteca Adafruit_ST7735
- Biblioteca EEPROM (compatível com ESP32)

Instale as bibliotecas acima pela Biblioteca do Arduino IDE ou via PlatformIO.

**Mapa de pinos (conforme código)**

- `TFT_CS` : 5
- `TFT_DC` : 2
- `TFT_RST`: 4
- `pinoBotaoNavegar`: 15 (INPUT_PULLUP)
- `pinoBotaoSelect` : 33 (INPUT_PULLUP)
- `pinoBuzzer` : 32 (OUTPUT)

**Uso rápido**

1. Abra o arquivo [v1_tomagoshi_code/v1_tomagoshi.ino](v1_tomagoshi_code/v1_tomagoshi.ino) no Arduino IDE ou PlatformIO.
2. Selecione a placa ESP32 correta e a porta serial.
3. Instale as dependências mencionadas.
4. Compile e faça upload para o ESP32.
5. Conecte o display ST7735 e os botões conforme o mapa de pinos.

**Modelos 3D / Impressão**

- Verifique a pasta `tomagoshi_3D_layout/` para os arquivos .3mf: `v3.tomagoshiRobotAssets.3mf` e `v3.tomagoshiRobotBody.3mf`. Ajuste configurações de impressão conforme sua impressora.

**Licença**

- Nenhuma licença especificada; adicione um arquivo LICENSE se desejar definir termos de uso.
- Give a star!

**Próximos passos sugeridos**

- Validar/atualizar o esquema de ligação do hardware com o protótipo real.
- Documentar versão de bibliotecas/testes e criar um `hardware.md` com fotos de montagem.
- Adicionar um arquivo `LICENSE` e issues/README para contribuições.

**Mídia (fotos e vídeos)**

- Adicionei imagens e vídeos à pasta `pics/` para documentação e referência:
  - [pics/tomagoshi_circuit_map_display.jpg](pics/tomagoshi_circuit_map_display.jpg) — Foto do mapa do display.
  - [pics/tomagoshi_circuit.mp4](pics/tomagoshi_circuit.mp4) — Vídeo do circuito.
  - [pics/tomagoshi_test.mp4](pics/tomagoshi_test.mp4) — Vídeo de teste do firmware.
  - [pics/tomagoshi_V3.mp4](pics/tomagoshi_V3.mp4) — Vídeo do modelo V3 / montagem 3D.

Use esses arquivos para ilustrar montagem, conectar o hardware e demonstrar o firmware em ação.

---

Arquivo atualizado automaticamente pelo assistente.
