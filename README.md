# Proyecto Compiladores

Compilador ChocoPy con scanner léxico, parser LR(1), construcción del AST y análisis semántico en C++, y un lanzador en Python.

## Compilación y uso

Se requiere CMake 3.16 o posterior, un compilador compatible con C++17 y Python 3.7 o posterior.
Desde la raíz del repositorio, ejecuta:

```text
python choco.py
```

El lanzador actualiza el ejecutable con CMake y abre el visualizador local. En la pestaña **Entrada**, haz clic en **Subir archivo** y elige un `.py` de hasta 1 MiB. El análisis se ejecuta al seleccionar el archivo. Puedes subir otro desde **Entrada** y cerrar el servidor con `Ctrl+C` en la terminal.

La gramática se incorpora al binario al compilar. El servidor local recibe el archivo del visualizador y ejecuta el compilador en un directorio temporal para cada análisis.

Después de construir el AST, el compilador registra las declaraciones del módulo, las clases y las funciones, y resuelve las referencias léxicas. Reporta redeclaraciones, nombres sin declaración y errores relacionados con `global` y `nonlocal`. Los atributos de clase se acceden por medio del receptor, por ejemplo `self.valor`.

Con un AST completo, el compilador comprueba los tipos de las expresiones, asignaciones, condiciones, argumentos y retornos. También valida el acceso a miembros, la indexación y la iteración. Si un nombre tiene un error de ámbito, el compilador evita los diagnósticos de tipo derivados de ese nombre y sigue comprobando las expresiones independientes.

La ejecución muestra un único reporte agrupado en errores léxicos, sintácticos y semánticos, con línea y columna cuando están disponibles. Los errores sintácticos incluyen los terminales esperados y el resumen muestra el total de cada fase. Un archivo vacío se acepta como un programa sin declaraciones ni sentencias.

El AST de un programa aceptado incluye las ubicaciones y los tipos inferidos. Las secuencias se anidan a la derecha como `Seq(a, Seq(b, c))`. Una secuencia vacía es un `Seq` sin hijos y una de un solo elemento se representa por ese elemento.

### Visualizador del compilador

Las pestañas muestran el código fuente con números de línea, los diagnósticos, los tokens, la traza LR(1), los ámbitos y el AST tipado. Selecciona un estado del autómata para consultar sus transiciones, sus ítems y las filas de ACTION y GOTO. Todos los desplegables aparecen expandidos al cargar el informe. Si se rechaza el programa, el visualizador muestra los diagnósticos y los resultados de las fases disponibles.

### Pruebas de integración

```text
cmake -E chdir build ctest --output-on-failure
```

Las pruebas cubren el reporte por fases, la recuperación sintáctica, el análisis semántico y la exportación del AST tipado.

La carpeta [ejemplos](./ejemplos/README.md) contiene entradas válidas y con errores para subir desde el visualizador.

## Módulos

- [Scanner](./scanner/README.md) — análisis léxico.
- [Parser](./parser/README.md) — generador de tablas LR(1) y visualizador.

## Estructura

    chocopy/
    ├── choco.py
    ├── CMakeLists.txt
    ├── ejemplos/
    ├── frontend/
    ├── scanner/
    ├── semantics/
    ├── parser/
    └── README.md

## Autores

- Daniela Perales Estrada
- José Gabriel Cornejo Castro
- María de los Angeles Vásquez Pineda
