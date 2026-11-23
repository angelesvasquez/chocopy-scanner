llamadas: int = 0

def acumular(inicio: int) -> int:
    total: int = 0
    def aumentar(paso: int) -> int:
        global llamadas
        nonlocal total
        total = total + paso
        llamadas = llamadas + 1
        return total
    total = inicio
    aumentar(2)
    return aumentar(3)

print(acumular(10))
print(llamadas)
