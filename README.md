# ChocoPy Scanner

Scanner léxico para el lenguaje **ChocoPy**, desarrollado en C++.

El proyecto identifica tokens del código fuente y reporta errores léxicos básicos, como caracteres no válidos, operadores inválidos, errores de indentación y cadenas sin cerrar.

## Estructura

```text
chocopy-scanner/
├── src/
│   ├── main.cpp
│   ├── scanner.cpp
│   └── scanner.h
└── tests/
```

## Compilación

Desde la carpeta `src/`:

```bash
g++ main.cpp scanner.cpp -o scanner
```

## Ejecución

El scanner utiliza un archivo `input.txt` como entrada.

```bash
./scanner
```

En Windows:

```powershell
.\scanner.exe
```

## Funcionalidades

El scanner reconoce, entre otros:

* Identificadores y palabras reservadas
* Números enteros
* Cadenas de texto
* Operadores y símbolos
* Comentarios
* Saltos de línea
* `INDENT` y `DEDENT`
* Errores léxicos

## Tests

Los casos de prueba se encuentran en la carpeta `tests/`.

Estos incluyen casos válidos y casos con errores léxicos, como cadenas sin cerrar, operadores inválidos y errores de indentación.

## Autores

- Daniela Perales estrada

- José Gabriel Cornejo Castro

- María de los Angeles Vásquez Pineda