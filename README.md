# Matterbloom

**Matterbloom** é um simples sandbox científico 2D em pixel art para PC.

## Instalação

Requer Windows e um compilador compatível com **C++17** (MSVC ou MinGW-w64).

1. Baixe o código-fonte.
2. Execute `Build.cmd` na pasta do projeto.
3. Aguarde a compilação e abra `Matterbloom.exe`.

O script verifica as ferramentas necessárias. A primeira compilação requer conexão com a internet para baixar as dependências.

**Compilação manual:**

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
```

## Como jogar

Escolha entre **Mundo natural** e **Laboratório**, configure o cenário e comece a experimentar. Utilize materiais, ferramentas e configurações para modificar o ambiente e observar suas interações.

| Comando | Ação |
|---|---|
| Clique esquerdo / direito | Pintar / apagar |
| Roda do mouse | Ajustar pincel |
| Ctrl + roda | Zoom |
| Botão central | Mover câmera |
| `B` / `H` / `C` / `E` / `I` | Ferramentas principais |
| `Espaço` / `N` | Pausar / avançar simulação |
| `Ctrl+Z` / `Ctrl+Y` | Desfazer / refazer |
| `Ctrl+S` / `Ctrl+O` | Salvar / abrir mundo |
| `F5` / `F9` | Salvamento rápido / carregar |
| `Esc` / `Tab` | Menu / ocultar interface |

## Documentação

Consulte a [documentação completa](docs/README.md) para conhecer os sistemas, ferramentas, configurações e limitações da simulação.

*Matterbloom utiliza aproximações físicas discretas e não substitui simulações científicas de engenharia.*
