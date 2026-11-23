class Producto(object):
    precio: int = 10
    def costo(self: "Producto", unidades: int) -> int:
        return self.precio * unidades

class ProductoConDescuento(Producto):
    def costo(self: "ProductoConDescuento", unidades: int) -> int:
        return self.precio * unidades - 2

compra: Producto = None

compra = ProductoConDescuento()
print(compra.costo(3))
