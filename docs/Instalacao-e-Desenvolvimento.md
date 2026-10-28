# Instalação e desenvolvimento

## Compilação no Windows

1. Instale um compilador compatível com **C++17** (Visual Studio Build Tools ou MinGW-w64).
2. Na raiz do projeto, execute **`Build.cmd`**.
3. O script procura CMake e Git, orienta a instalação se necessário e compila o executável.
4. Abra **`Matterbloom.exe`** na pasta de saída indicada.

A primeira compilação requer internet para baixar SFML, Dear ImGui, ImGui-SFML e nlohmann/json. O CMake copia `assets/`, `materials.json` e `reactions.json` para junto do executável.

Alternativa pelo terminal:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
```

## Estrutura do código

| Arquivo | Responsabilidade |
| --- | --- |
| `src/App.cpp` | Interface, entrada, áudio e ferramentas |
| `src/WorldEngine.cpp` | Atualização do mundo e salvamento |
| `src/WorldPhysics.cpp` | Forças, fluidos, colisões e estruturas |
| `src/WorldEnvironment.cpp` | Água no solo, pressão e clima |
| `src/WorldArt.cpp` | Paisagem e arte |
| `src/Biology.cpp` | Vegetação e crescimento |
| `src/MaterialDB.cpp` | Catálogo e reações |
| `materials.json` / `reactions.json` | Materiais e transformações |

## Verificar o motor

Os testes da pasta `tests/` verificam física, química, ecologia, laboratório, configurações e salvamento. Eles usam representações simplificadas da interface, portanto **não substituem** um teste visual do jogo.

Dois scripts opcionais automatizam essa verificação:

- `scripts/test-headless.sh`: compila e executa testes sem abrir a interface, usando Bash e `g++`.
- `scripts/test-windows.cmd`: executa os testes no Windows, usando `g++`.

Para usá-los, execute `bash scripts/test-headless.sh` ou `scripts\test-windows.cmd`. Para validar o lançamento, abra o executável real, teste gráficos, som, entrada do mouse, criação de mundos e salvamentos.
