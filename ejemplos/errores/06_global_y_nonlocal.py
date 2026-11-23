contador: int = 0

def exterior():
    global ausente
    def interior():
        nonlocal contador
        contador = contador + 1
    ausente = 1
    interior()

exterior()
