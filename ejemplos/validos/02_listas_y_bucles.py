valores: [int] = None

def sumar(numeros: [int]) -> int:
    numero: int = 0
    total: int = 0
    for numero in numeros:
        total = total + numero
    return total

valores = [2, 4] + [6, 8]
valores[0] = valores[0] + 1
print(sumar(valores))
