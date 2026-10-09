# Generador LR(1) - Parser

Generador de tablas LR(1), en C++. Dada una gramática libre
de contexto, construye la colección canónica de ítems LR(1), calcula los
conjuntos FIRST y genera las tablas ACTION y GOTO, reportando conflictos
shift/reduce y reduce/reduce. Exporta el autómata y las tablas para el
visualizador web.

## Estructura

    parser/
    ├── src/
    │   ├── Grammar.cpp
    │   ├── LR1.cpp
    │   └── input.txt
    ├── tests/
    ├── visualizer/
    │   └── index.html
    └── README.md

- `Grammar.cpp` — lectura de la gramática y tabla de símbolos.
- `LR1.cpp` — FIRST, clausura, GOTO, colección canónica y tablas.
- `input.txt` — gramática de entrada.
- `tests/` — gramáticas de prueba.
- `visualizer/` — página web del autómata, tabla y conflictos.

## Compilación

Desde `src/`:

    g++ LR1.cpp -o lr1

## Ejecución

Usa `input.txt` como entrada:

    ./lr1

Imprime gramática aumentada, FIRST, colección canónica, transiciones y
tablas ACTION/GOTO. Exporta a `../visualizer/lr1.json` y
`../visualizer/data.js`. Devuelve `0` sin conflictos y `1` si los hay.

## Funcionalidades

- Gramáticas libres de contexto, incluidas producciones vacías.
- Gramática aumentada generada automáticamente: a partir del símbolo
  inicial de la primera producción (por ejemplo `List`), el generador
  crea `List'` con la producción `List' -> List` y agrega el terminal
  reservado `eof`. No deben escribirse en `input.txt`.
- FIRST por punto fijo, con ε para no terminales anulables.
- FIRST de cadenas β = s₁s₂…sₖ.
- Clausura y GOTO de ítems LR(1).
- Colección canónica CC = {cc₀, cc₁, …} con transiciones.
- Tablas ACTION y GOTO.
- Detección de conflictos shift/reduce y reduce/reduce con estado,
  terminal e ítems causantes.
- Exportación del autómata y las tablas para el visualizador.

## Formato de la gramática

    A -> α β γ

- El lado izquierdo es un no terminal.
- `->` separa izquierda y derecha.
- Los símbolos se separan por espacios.
- Producción vacía: nada a la derecha, o `''`.
- La primera producción define el símbolo inicial.
- `eof` es reservado y lo agrega la herramienta.

Ejemplo (gramática de paréntesis, símbolo inicial `List`):

    List -> List Pair
    List -> Pair
    Pair -> ( Pair )
    Pair -> ( )

La gramática aumentada que construye el generador es:

    List' -> List

## Visualizador

`visualizer/index.html` carga automáticamente `data.js` (funciona con
`file://`), con `lr1.json` como respaldo. Muestra el autómata, los ítems
agrupados por núcleo con sus lookaheads, la tabla ACTION/GOTO con celdas
en conflicto resaltadas y los conflictos con sus causas.

## Tests

En `tests/` hay gramáticas de paréntesis, conflicto shift/reduce,
conflicto reduce/reduce y producciones vacías. Para probar una, copiar su
`input.txt` a `src/` y ejecutar el parser.

## Autores

- Daniela Perales Estrada
- José Gabriel Cornejo Castro
- María de los Angeles Vásquez Pineda