# Materiais e química

Cada célula contém um material. O arquivo `materials.json` define nomes, cores, estados e propriedades; `reactions.json` define transformações por contato.

## Estados e propriedades

| Estado | Exemplos de comportamento |
| --- | --- |
| Sólido | Apoio, queda em conjuntos, dano e ruptura |
| Granular | Queda e acomodação, como areia e cascalho |
| Líquido | Escoamento, pressão, viscosidade e dispersão |
| Gás | Difusão, subida e deslocamento pelo vento |
| Plasma | Calor e interações com combustíveis, como fogo |

As propriedades incluem densidade, atrito, coesão, porosidade, permeabilidade, viscosidade, tensão superficial, compressibilidade, condutividade térmica e capacidade de armazenar calor.

## Temperatura e transformações

O calor passa entre células e é transportado pelo movimento das partículas. Materiais podem mudar de estado quando atingem determinadas condições. Exemplos:

- Água pode congelar ou evaporar.
- Lava em contato com água pode gerar vapor e obsidiana.
- Areia aquecida pode virar vidro fundido e solidificar depois.
- Madeira pode inflamar e produzir carvão ou cinzas.
- Metais podem fundir, resfriar ou reagir com substâncias corrosivas.

As transformações possuem probabilidades. Uma reação pode depender de temperatura e umidade, por isso nem todo contato gera resultado imediato.

## Observação

Use **Aquecer** (`H`), **Resfriar** (`C`), os mapas científicos e o inspetor para acompanhar as mudanças. As temperaturas internas são normalizadas e convertidas para unidades conhecidas apenas para facilitar a leitura.

**Importante:** o modelo representa química e calor de forma qualitativa. Não calcula concentrações moleculares ou conservação rigorosa de massa e energia.
