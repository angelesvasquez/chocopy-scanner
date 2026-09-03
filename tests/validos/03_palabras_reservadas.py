class animal(object):
    makes_noise: bool = False

    def sound(self: "animal") -> str:
        if self.makes_noise:
            return "algo"
        elif not self.makes_noise:
            pass
        else:
            return "nada"
        return "???"

for i in [1, 2, 3]:
    while i > 0:
        break
    continue

x: int = None
y: bool = True
z: bool = False
