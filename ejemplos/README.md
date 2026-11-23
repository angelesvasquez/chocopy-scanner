# Ejemplos de entrada

Desde la raíz del repositorio, pasa cualquier ejemplo al lanzador:

```powershell
python choco.py ejemplos/validos/04_clases_y_herencia.py
python choco.py ejemplos/errores/03_lexico_y_sintactico.py
```

Recarga el [visualizador](../frontend/visualizer/index.html) después de cada prueba para ver el informe de la última ejecución.

## Programas válidos

Estos ejemplos se aceptan con código de salida `0`.

| Archivo | Qué puedes inspeccionar |
|---|---|
| [01_expresiones.py](validos/01_expresiones.py) | Precedencia, operaciones booleanas y expresión condicional. |
| [02_listas_y_bucles.py](validos/02_listas_y_bucles.py) | Concatenación de listas, indexación, asignación de elementos y un bucle `for`. |
| [03_ambitos_anidados.py](validos/03_ambitos_anidados.py) | Una función anidada que modifica una variable local externa y una global. |
| [04_clases_y_herencia.py](validos/04_clases_y_herencia.py) | Herencia, acceso a un atributo y redefinición de un método. |

## Programas con errores

Estos ejemplos se rechazan con código de salida `1`.

| Archivo | Diagnósticos esperados |
|---|---|
| [01_lexico.py](errores/01_lexico.py) | Carácter inválido `@`. |
| [02_sintactico.py](errores/02_sintactico.py) | Falta la expresión a la derecha de `=`. |
| [03_lexico_y_sintactico.py](errores/03_lexico_y_sintactico.py) | Carácter inválido y asignación incompleta, agrupados por fase. |
| [04_ambitos.py](errores/04_ambitos.py) | Escritura a `saldo` sin `global` y uso de `saldo_final` sin declarar. |
| [05_tipos.py](errores/05_tipos.py) | Retorno, condición, operandos y argumento de tipos incompatibles. |
| [06_global_y_nonlocal.py](errores/06_global_y_nonlocal.py) | `global` refiere a un nombre ausente y `nonlocal` intenta enlazar una variable del módulo. |
