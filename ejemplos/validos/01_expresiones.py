precio: int = 12
unidades: int = 3
total: int = 0
mensaje: str = "Total"
hay_descuento: bool = False

total = precio * unidades
hay_descuento = total >= 30 and unidades > 1
total = total - 5 if hay_descuento else total
print(mensaje + ":")
print(total)
