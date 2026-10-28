# Matterbloom

**Matterbloom** é um sandbox científico 2D em pixel art para PC. Crie terrenos, construa estruturas, misture materiais e observe a interação entre gravidade, líquidos, calor, química, clima e vegetação. Jogue livremente em um mundo natural ou prepare experimentos em um laboratório.

## Instalação no Windows

O projeto é distribuído como código-fonte. Para gerar o executável:

1. Instale um compilador C++ com suporte a C++17, como o **Visual Studio Build Tools** (desenvolvimento para desktop com C++) ou o **MinGW-w64**.
2. Na pasta do projeto, execute **`Build.cmd`**. O script procura CMake e Git e orienta a instalação caso estejam ausentes.
3. Aguarde a compilação. Na primeira execução, é necessário acesso à internet para obter as bibliotecas do projeto.
4. Abra **`Matterbloom.exe`** na pasta de compilação indicada pelo script. Mantenha `materials.json`, `reactions.json` e `assets/` junto ao executável; a compilação já os copia automaticamente.

Como alternativa, compile pelo terminal:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
```

Se o executável não iniciar, confirme que a compilação terminou sem erros e que as dependências de execução do compilador estão instaladas.

## Como jogar

1. Na tela inicial, escolha **Mundo natural** ou **Laboratório**, o tamanho do mapa e um cenário.
2. Clique em **Gerar novo mundo** ou **Entrar no mundo**.
3. Selecione um material na paleta inferior e pinte com o mouse. Teste água, areia, fogo, rochas, plantas e outros materiais.
4. Abra **Ferramentas** para aquecer, resfriar, fixar estruturas, aplicar forças e lançar materiais.
5. Use **Mundo** para alterar clima, vento e visualizações; use **Ajustes** para controlar tempo, gravidade e gráficos.
6. Salve o experimento pela interface ou com `F5`. Os mundos usam arquivos `.mbw`.

| Comando | Ação |
| --- | --- |
| Clique esquerdo / direito | Pintar / apagar no modo de edição |
| Roda do mouse / Ctrl + roda | Tamanho do pincel / zoom |
| Botão central arrastado | Mover a câmera |
| Alt + clique esquerdo | Selecionar o material sob o cursor |
| `B`, `H`, `C`, `E`, `I` | Material, aquecer, resfriar, apagar, inspecionar |
| `Espaço` / `N` | Pausar ou continuar / avançar um passo quando pausado |
| `Esc` / `Tab` | Menu de pausa / ocultar a interface |
| `Ctrl+Z` / `Ctrl+Y` | Desfazer / refazer |
| `Ctrl+S` / `Ctrl+O` | Salvar / abrir arquivo de mundo |
| `F5` / `F9` | Salvar no slot / carregar mundo salvo |

Para **mirar e lançar**, escolha Lançador, Impulso, Ventilador, Jato, Atração ou Explosão, mantenha o botão direito pressionado, arraste na direção oposta ao efeito e clique com o esquerdo. A roda ajusta a potência durante a mira; `Shift` alterna o disparo contínuo.

## Documentação

Consulte o [guia completo por módulos](docs/README.md) para aprender sobre todos os sistemas, configurações, experimentos, salvamentos, testes e limitações científicas.

Matterbloom é uma simulação discreta orientada à experimentação e à jogabilidade; suas grandezas físicas são aproximações, não resultados de engenharia.
