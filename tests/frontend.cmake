file(MAKE_DIRECTORY "${WORK_DIR}")

function(run_compiler expected)
    execute_process(
        COMMAND "${COMPILER}" ${ARGN}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        TIMEOUT 30
    )
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "Expected exit ${expected}, got ${result} for ${ARGN}\n${output}${errors}")
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
    reject_part("${RESULT_TEXT}" "Program [")
    run_compiler(1 mixed.py --trace)
    expect_parts("${RESULT_TEXT}" "SEGUIMIENTO DEL ANALISIS" "${expected_summary}")

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
    expect_parts("${RESULT_TEXT}" "Resultado: ACEPTADO" "Program [1:1]")
    run_compiler(0 empty.py --report-json empty.json)
    file(READ "${WORK_DIR}/empty.json" empty_report)
    expect_parts("${empty_report}" "\"sourceText\": \"\"" "\"accepted\": true")
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
    run_compiler(0 "programa valido.py" --ast-json ast.json)
    expect_parts("${RESULT_TEXT}" "Resultado: ACEPTADO" "AST guardado en ast.json"
        "errores lexicos: 0, errores sintacticos: 0, errores semanticos: 0")
    file(READ "${WORK_DIR}/ast.json" json)
    file(READ "${WORK_DIR}/ast-data.js" script)
    expect_parts("${json}" "\"source\": \"programa valido.py\"" "\"kind\":\"Program\""
        "\"kind\":\"NonlocalDecl\"" "\"kind\":\"GlobalDecl\"" "\"kind\":\"BinOp\""
        "\"type\":\"int\"" "\"type\":\"bool\"")
    if(NOT "${script}" STREQUAL "globalThis.chocopyAst = ${json};\n")
        message(FATAL_ERROR "The viewer companion and the exported AST differ")
    endif()

    file(WRITE "${WORK_DIR}/rechazado.py" "x: int = False\n")
    run_compiler(1 rechazado.py --ast-json ast.json)
    expect_parts("${RESULT_TEXT}" "Inicializacion de 'x'" "Resultado: RECHAZADO")
    reject_part("${RESULT_TEXT}" "AST guardado")
    file(READ "${WORK_DIR}/ast.json" after_json)
    file(READ "${WORK_DIR}/ast-data.js" after_script)
    if(NOT "${after_json}" STREQUAL "${json}" OR NOT "${after_script}" STREQUAL "${script}")
        message(FATAL_ERROR "Rejected input modified a previously valid AST")
    endif()

    run_compiler(0 "programa valido.py" --report-json report.json)
    expect_parts("${RESULT_TEXT}" "Resultado: ACEPTADO" "Informe guardado en report.json")
    reject_part("${RESULT_TEXT}" "SEGUIMIENTO DEL ANALISIS")
    file(READ "${WORK_DIR}/report.json" report)
    file(READ "${WORK_DIR}/report-data.js" report_script)
    string(REPLACE "\\u000d\\u000a" "\\u000a" report_lines "${report}")
    expect_parts("${report_lines}" "\"sourceText\": \"total: int = 0\\u000avalue: Child = None" "\"accepted\": true"
        "\"tokens\": [{" "\"trace\": [{" "\"states\":[{"
        "\"action\":[{" "\"goto\":[{" "\"scopes\": [{" "\"name\":\"count\",\"kind\":\"nonlocal\""
        "\"kind\":\"Program\"" "\"type\":\"int\"")
    if(NOT "${report_script}" STREQUAL "globalThis.chocopyReport = ${report};\n")
        message(FATAL_ERROR "The compilation report and its viewer data differ")
    endif()

    run_compiler(1 rechazado.py --report-json report.json)
    file(READ "${WORK_DIR}/report.json" rejected_report)
    expect_parts("${rejected_report}" "\"accepted\": false" "Inicializacion de 'x'" "\"scopes\": [{" "\"kind\":\"Program\"")
    file(WRITE "${WORK_DIR}/incompleto.py" "x =\npass $\n")
    run_compiler(1 incompleto.py --report-json incomplete.json)
    file(READ "${WORK_DIR}/incomplete.json" incomplete_report)
    string(REPLACE "\\u000d\\u000a" "\\u000a" incomplete_report "${incomplete_report}")
    expect_parts("${incomplete_report}" "\"accepted\": false" "caracter no valido" "Token inesperado"
        "\"sourceText\": \"x =\\u000apass $\\u000a\"" "\"symbol\":\"eof\"" "\"ast\": null" "\"scopes\": []")
    file(READ "${WORK_DIR}/ast.json" after_report)
    if(NOT "${after_report}" STREQUAL "${json}")
        message(FATAL_ERROR "The compilation report changed the last valid AST export")
    endif()

    run_compiler(1 missing.py --report-json report.json)
    expect_parts("${RESULT_TEXT}" "No se pudo abrir el archivo 'missing.py'" "Resultado: RECHAZADO"
        "Informe guardado en report.json")
    reject_part("${RESULT_TEXT}" "Error del compilador")
    file(READ "${WORK_DIR}/report.json" missing_report)
    file(READ "${WORK_DIR}/report-data.js" missing_script)
    expect_parts("${missing_report}" "\"source\": \"missing.py\"" "\"accepted\": false"
        "\"sourceText\": null" "No se pudo abrir el archivo" "\"automaton\": null" "\"ast\": null")
    if(NOT "${missing_script}" STREQUAL "globalThis.chocopyReport = ${missing_report};\n")
        message(FATAL_ERROR "The viewer data did not receive the missing-source report")
    endif()

    run_compiler(2 missing.py --report-json ./missing.py)
    expect_parts("${RESULT_TEXT}" "coincide con el archivo fuente")
    if(EXISTS "${WORK_DIR}/missing.py")
        message(FATAL_ERROR "The compiler created a report at the missing source path")
    endif()

    file(WRITE "${WORK_DIR}/secuencia.py" "pass\npass\npass\n")
    run_compiler(0 secuencia.py --ast-json secuencia.json)
    file(READ "${WORK_DIR}/secuencia.json" sequence_json)
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
