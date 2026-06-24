# Tomagoshi ESP32

Projeto de um tamagotchi em estilo retrô para placas ESP32, com display ST7735 de 128x160 pixels, botões físicos, buzzer e persistência em EEPROM. O repositório reúne o firmware, os arquivos de montagem 3D e materiais visuais para documentação.

## O que existe neste projeto

- Firmware para ESP32 em Arduino/ESP-IDF style com lógica de estados do mascote.
- Interface simples no display com barras de status para energia, felicidade e fome.
- Estados visuais do personagem, incluindo faces de felicidade, tristeza, sono e doença.
- Mini-jogo de corrida/jumping com persistência do melhor score.
- Arquivos 3D para montagem do corpo/partes do robô e mídia de referência para o protótipo.

## Estrutura do repositório

- [v1_tomagoshi_code/v1_tomagoshi.ino](v1_tomagoshi_code/v1_tomagoshi.ino): firmware principal do projeto.
- [tomagoshi_3D_layout](tomagoshi_3D_layout): modelos 3D em formato .3mf.
- [pics](pics): fotos e vídeos de montagem, circuito e testes.

## Funcionalidades implementadas

- Exibição do personagem no display ST7735.
- Controle de três atributos principais:
  - energia
  - felicidade
  - fome
- Salvamento automático de estado na EEPROM.
- Relógio interno simples exibido na tela.
- Menu interativo com ações de navegação e seleção.
- Mini-jogo com obstáculos e pontuação.
- Feedback sonoro via buzzer.

## Requisitos de hardware

Componentes recomendados:

- Placa ESP32 (pode ser um módulo DevKitC ou similar).
- Display TFT ST7735 de 128x160.
- 2 botões push-button (navegação e seleção).
- Buzzer ativo ou passivo.
- Fonte de alimentação adequada para a placa e o display.
- Fios de conexão e protoboard ou PCB para montagem.

## Mapa de pinos utilizado no firmware

O código atual usa os seguintes pinos:

- `TFT_CS`: 5
- `TFT_DC`: 2
- `TFT_RST`: 4
- `pinoBotaoNavegar`: 15 (entrada com `INPUT_PULLUP`)
- `pinoBotaoSelect`: 33 (entrada com `INPUT_PULLUP`)
- `pinoBuzzer`: 32 (saída)

> Ajuste o mapeamento se o seu hardware usar outro layout de conexão.

## Dependências de software

Instale estas bibliotecas antes de compilar:

- Adafruit_GFX
- Adafruit_ST7735
- EEPROM (já disponível em grande parte das placas ESP32)

Você pode instalar as bibliotecas pelo gerenciador de bibliotecas do Arduino IDE ou via PlatformIO.

## Como compilar e enviar para a placa

1. Abra [v1_tomagoshi_code/v1_tomagoshi.ino](v1_tomagoshi_code/v1_tomagoshi.ino) no Arduino IDE ou PlatformIO.
2. Selecione a placa ESP32 correta e a porta serial correspondente.
3. Verifique se as bibliotecas listadas acima estão instaladas.
4. Compile o projeto.
5. Faça o upload para a ESP32.
6. Conecte o display, os botões e o buzzer conforme o mapa de pinos.

## Comportamento esperado

Ao ligar a placa, o firmware inicia o mascote e exibe o estado atual. O usuário pode interagir com o menu e com as ações do tamagotchi. O sistema também salva dados na EEPROM para preservar o estado entre reinicializações.

## Arquivos 3D e impressão

A pasta [tomagoshi_3D_layout](tomagoshi_3D_layout) contém os modelos 3D em formato .3mf:

- [tomagoshi_3D_layout/v3.tomagoshiRobotAssets.3mf](tomagoshi_3D_layout/v3.tomagoshiRobotAssets.3mf)
- [tomagoshi_3D_layout/v3.tomagoshiRobotBody.3mf](tomagoshi_3D_layout/v3.tomagoshiRobotBody.3mf)

Ajuste as configurações de impressão conforme a sua impressora e os materiais usados.

## Mídia e referências visuais

Os arquivos em [pics](pics) servem como referência de montagem, conexão e teste:

- [pics/tomagoshi_circuit_map_display.jpg](pics/tomagoshi_circuit_map_display.jpg): mapa do circuito/display.
- [pics/tomagoshi_circuit.mp4](pics/tomagoshi_circuit.mp4): vídeo do circuito.
- [pics/tomagoshi_test.mp4](pics/tomagoshi_test.mp4): vídeo de teste do firmware.
- [pics/tomagoshi_V3.mp4](pics/tomagoshi_V3.mp4): vídeo do modelo V3 e montagem 3D.

## Próximos passos sugeridos

- Validar o esquema de ligação com o protótipo físico real.
- Documentar a versão exata das bibliotecas e o resultado de testes.
- Criar um documento adicional de montagem com fotos e instruções mais detalhadas.
- Adicionar um arquivo de licença e um guia de contribuição.

## Licença

Ainda não há um arquivo de licença definido neste repositório. Se quiser, pode adicionar um arquivo LICENSE para formalizar o uso e a distribuição do projeto.
