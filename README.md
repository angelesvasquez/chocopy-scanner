# Proyecto Compiladores

Compilador ChocoPy con scanner léxico, parser LR(1), construcción del AST y análisis semántico en C++, y un lanzador en Python.

## Compilación y uso

Se requiere CMake 3.16 o posterior, un compilador compatible con C++17 y Python 3.
Desde la raíz del repositorio, compila una vez y ejecuta el lanzador con el programa fuente:

```text
cmake -S . -B build
cmake --build build
python choco.py scanner/tests/validos/01_funcion_basica.py
```

Agrega `--trace` para ver la traza LR(1). La gramática se incorpora al binario al compilar.
Desde otro directorio, usa la ruta al lanzador y la ruta al archivo fuente.
El código de salida es `0` si el análisis acepta el programa, `1` si lo rechaza o el archivo no está disponible y `2` si falla la invocación.

Después de construir el AST, el compilador registra las declaraciones del módulo, las clases y las funciones, y resuelve las referencias léxicas. Reporta redeclaraciones, nombres sin declaración y errores relacionados con `global` y `nonlocal`. Los atributos de clase se acceden por medio del receptor, por ejemplo `self.valor`.

Con un AST completo, el compilador comprueba los tipos de las expresiones, asignaciones, condiciones, argumentos y retornos. También valida el acceso a miembros, la indexación y la iteración. Si un nombre tiene un error de ámbito, el compilador evita los diagnósticos de tipo derivados de ese nombre y sigue comprobando las expresiones independientes.

La ejecución muestra un único reporte agrupado en errores léxicos, sintácticos y semánticos, con línea y columna cuando están disponibles. Los errores sintácticos incluyen los terminales esperados y el resumen muestra el total de cada fase. Un archivo vacío se acepta como un programa sin declaraciones ni sentencias.

El AST de un programa aceptado incluye las ubicaciones y los tipos inferidos. Las secuencias se anidan a la derecha como `Seq(a, Seq(b, c))`. Una secuencia vacía es un `Seq` sin hijos y una de un solo elemento se representa por ese elemento.

### Visualizador del compilador

```text
python choco.py scanner/tests/validos/10_bloques_anidados.py
```

Abre [frontend/visualizer/index.html](./frontend/visualizer/index.html). Las pestañas muestran el código fuente con números de línea, los diagnósticos, los tokens, el análisis LR(1), los ámbitos y el AST tipado. Selecciona un estado del autómata para consultar sus transiciones, sus ítems y las filas de ACTION y GOTO. Las ramas del AST aparecen expandidas al cargar el informe.

El lanzador guarda el informe en `frontend/visualizer/report.json` y genera `report-data.js` para que el HTML también pueda abrirse con doble clic. El destino predeterminado se calcula desde la ubicación del lanzador. El informe se guarda también si el programa se rechaza y el compilador conserva el código de salida `1`. Con errores léxicos o sintácticos, la página muestra las fases disponibles.

Usa `--report-json RUTA` para cambiar el destino del informe o `--ast-json RUTA` para exportar solo el AST de un programa aceptado. El visualizador usa `ast.json` cuando no encuentra un informe completo. Agrega `--trace` para imprimir la traza también en la consola.

### Pruebas de integración

```text
cmake -E chdir build ctest --output-on-failure
```

Las pruebas cubren el reporte por fases, la recuperación sintáctica, el análisis semántico y la exportación del AST tipado.

La carpeta [ejemplos](./ejemplos/README.md) contiene entradas válidas y con errores para probar el compilador desde el lanzador.

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
