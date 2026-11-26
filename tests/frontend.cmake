file(MAKE_DIRECTORY "${WORK_DIR}")

function(run_compiler expected source)
    file(REMOVE "${WORK_DIR}/programa.py" "${WORK_DIR}/report.json")
    if(EXISTS "${WORK_DIR}/${source}")
        configure_file("${WORK_DIR}/${source}" "${WORK_DIR}/programa.py" COPYONLY)
    endif()
    execute_process(
        COMMAND "${COMPILER}"
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 30
    )
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "Expected exit ${expected}, got ${result} for ${source}\n${output}${errors}")
    endif()
    set(RESULT_TEXT "${output}${errors}" PARENT_SCOPE)
endfunction()

function(expect_parts text)
    math(EXPR last "${ARGC} - 1")
    foreach(index RANGE 1 ${last})
        set(part "${ARGV${index}}")
        string(FIND "${text}" "${part}" position)
        if(position EQUAL -1)
            string(SUBSTRING "${text}" 0 1600 excerpt)
            message(FATAL_ERROR "Missing '${part}' in (first 1600 characters):\n${excerpt}")
        endif()
    endforeach()
endfunction()

function(reject_part text part)
    string(FIND "${text}" "${part}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "Unexpected '${part}' in:\n${text}")
    endif()
endfunction()

if(SUITE STREQUAL "diagnostics")
    file(WRITE "${WORK_DIR}/mixed.py" [=[
x =
print(1)
y =
pass $
]=])
    run_compiler(1 mixed.py)
    set(expected_summary "Resumen: errores lexicos: 1, errores sintacticos: 2, errores semanticos: 0")
    expect_parts("${RESULT_TEXT}" "--- DIAGNOSTICOS ---" "Errores lexicos"
        "Linea 4, col 6" "Errores sintacticos" "Linea 1, col 5" "Linea 3, col 5"
        "Token inesperado" "Se esperaba: 'ID'" "${expected_summary}" "Resultado: RECHAZADO")
    reject_part("${RESULT_TEXT}" "Nombre 'x' no declarado")
    reject_part("${RESULT_TEXT}" "SEGUIMIENTO DEL ANALISIS")

    file(WRITE "${WORK_DIR}/early.py" "    pass\npass @\n")
    run_compiler(1 early.py)
    expect_parts("${RESULT_TEXT}" "Linea 2, col 6" "Token inesperado 'INDENT'"
        "errores lexicos: 1, errores sintacticos: 1, errores semanticos: 0")

    run_compiler(1 missing.py)
    expect_parts("${RESULT_TEXT}" "No se pudo abrir el archivo" "Resultado: RECHAZADO"
        "errores lexicos: 1, errores sintacticos: 0, errores semanticos: 0")
    reject_part("${RESULT_TEXT}" "Linea 0")
    file(WRITE "${WORK_DIR}/empty.py" "")
    run_compiler(0 empty.py)
    expect_parts("${RESULT_TEXT}" "Resultado: ACEPTADO")
    file(READ "${WORK_DIR}/report.json" empty_report)
    expect_parts("${empty_report}" "\"sourceText\": \"\"" "\"accepted\": true")

    file(WRITE "${WORK_DIR}/comments.py"
        "# texto de prueba\ndef f() -> int:\n    # texto de prueba\n\n    return 1  # texto de prueba\n# texto de prueba")
    run_compiler(0 comments.py)
    file(READ "${WORK_DIR}/report.json" comments_report)
    expect_parts("${comments_report}" "\"accepted\": true" "\"kind\":\"Return\"")
    reject_part("${comments_report}" "\"text\":\"texto de prueba\"")
elseif(SUITE STREQUAL "semantics")
    file(WRITE "${WORK_DIR}/independent.py" [=[
x: int = 1
x: int = 2
print(falta + 1)
x = True
]=])
    run_compiler(1 independent.py)
    expect_parts("${RESULT_TEXT}" "Errores semanticos" "Linea 2, col 1: Nombre 'x' redeclarado"
        "Linea 3, col 7: Nombre 'falta' no declarado" "Linea 4, col 1: Asignacion incompatible"
        "errores semanticos: 3")
    reject_part("${RESULT_TEXT}" "Operador '+' incompatible")

    file(WRITE "${WORK_DIR}/checks.py" [=[
x: int = 0
def f(p: int) -> bool:
    x = 2
    return p
if 1:
    pass
f()
f("str")
x = 1 + "str"
x[0]
for x in False:
    pass
return
]=])
    run_compiler(1 checks.py)
    expect_parts("${RESULT_TEXT}" "requiere declarar 'global x'" "Retorno incompatible"
        "Condicion invalida" "espera 1 argumento(s), recibio 0" "Argumento 1 de 'f'"
        "Operador '+' incompatible" "Solo se puede indexar" "Solo se puede iterar"
        "'return' fuera de una funcion" "errores semanticos: 9")

    file(WRITE "${WORK_DIR}/directives.py" [=[
x: int = 0
def f():
    global missing
    def g():
        nonlocal x
        x = x + 1
    missing = missing + 1
    g()
]=])
    run_compiler(1 directives.py)
    expect_parts("${RESULT_TEXT}" "'global missing' requiere" "'nonlocal x' requiere" "errores semanticos: 2")
elseif(SUITE STREQUAL "export")
    file(WRITE "${WORK_DIR}/programa valido.py" [=[
total: int = 0
value: Child = None
class Base(object):
    amount: int = 2
    def get(self: "Base") -> int:
        return self.amount
class Child(Base):
    pass
def update(start: int) -> int:
    count: int = 1
    def step(delta: int) -> int:
        global total
        nonlocal count
        count = count + delta
        total = count
        return total
    return step(start)
value = Child()
if value.get() > 0:
    total = update(value.get())
print(total)
]=])
    run_compiler(0 "programa valido.py")
    expect_parts("${RESULT_TEXT}" "Resultado: ACEPTADO" "Informe guardado en report.json"
        "errores lexicos: 0, errores sintacticos: 0, errores semanticos: 0")
    reject_part("${RESULT_TEXT}" "SEGUIMIENTO DEL ANALISIS")
    file(READ "${WORK_DIR}/report.json" report)
    string(REPLACE "\\u000d\\u000a" "\\u000a" report_lines "${report}")
    expect_parts("${report_lines}" "\"sourceText\": \"total: int = 0\\u000avalue: Child = None" "\"accepted\": true"
        "\"tokens\": [{" "\"trace\": [{" "\"states\":[{"
        "\"action\":[{" "\"goto\":[{" "\"scopes\": [{" "\"name\":\"count\",\"kind\":\"nonlocal\""
        "\"source\": \"programa.py\"" "\"kind\":\"Program\"" "\"kind\":\"NonlocalDecl\""
        "\"kind\":\"GlobalDecl\"" "\"kind\":\"BinOp\"" "\"type\":\"int\"" "\"type\":\"bool\"")

    file(WRITE "${WORK_DIR}/rechazado.py" "x: int = False\n")
    run_compiler(1 rechazado.py)
    file(READ "${WORK_DIR}/report.json" rejected_report)
    expect_parts("${rejected_report}" "\"accepted\": false" "Inicializacion de 'x'" "\"scopes\": [{" "\"kind\":\"Program\"")
    file(WRITE "${WORK_DIR}/incompleto.py" "x =\npass $\n")
    run_compiler(1 incompleto.py)
    file(READ "${WORK_DIR}/report.json" incomplete_report)
    string(REPLACE "\\u000d\\u000a" "\\u000a" incomplete_report "${incomplete_report}")
    expect_parts("${incomplete_report}" "\"accepted\": false" "caracter no valido" "Token inesperado"
        "\"sourceText\": \"x =\\u000apass $\\u000a\"" "\"symbol\":\"eof\"" "\"ast\": null" "\"scopes\": []")
    run_compiler(1 missing.py)
    expect_parts("${RESULT_TEXT}" "No se pudo abrir el archivo 'programa.py'" "Resultado: RECHAZADO"
        "Informe guardado en report.json")
    reject_part("${RESULT_TEXT}" "Error del compilador")
    file(READ "${WORK_DIR}/report.json" missing_report)
    expect_parts("${missing_report}" "\"source\": \"programa.py\"" "\"accepted\": false"
        "\"sourceText\": null" "No se pudo abrir el archivo" "\"automaton\": null" "\"ast\": null")

    if(EXISTS "${WORK_DIR}/programa.py")
        message(FATAL_ERROR "The compiler created a source file for a missing input")
    endif()

    file(WRITE "${WORK_DIR}/secuencia.py" "pass\npass\npass\n")
    run_compiler(0 secuencia.py)
    file(READ "${WORK_DIR}/report.json" sequence_json)
    set(sequence_prefix [=["kind":"Seq","text":"statements"]=])
    set(first_stmt [=[{"kind":"Pass","text":"","line":1,"col":1,"type":null,"children":[]}]=])
    set(second_stmt [=[{"kind":"Pass","text":"","line":2,"col":1,"type":null,"children":[]}]=])
    set(third_stmt [=[{"kind":"Pass","text":"","line":3,"col":1,"type":null,"children":[]}]=])
    expect_parts("${sequence_json}"
        "${sequence_prefix},\"line\":1,\"col\":1,\"type\":null,\"children\":[${first_stmt},{${sequence_prefix},\"line\":2,\"col\":1,\"type\":null,\"children\":[${second_stmt},${third_stmt}]}]"
        "\"kind\":\"Seq\",\"text\":\"declarations\",\"line\":0,\"col\":0,\"type\":null,\"children\":[]")
else()
    message(FATAL_ERROR "Unknown test suite: ${SUITE}")
endif()
