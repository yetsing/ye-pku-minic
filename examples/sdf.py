import math, time, typing

n = 0
scale = 10000

def fraction(n: int, scale: int) -> int:
    if n < 0:
        n = -n
    n = int(n * scale % scale)
    return n


def donut(x: float, y: float, z: float) -> float:
    radius = 0.4
    thickness = 0.3
    res = math.sqrt((math.sqrt(x**2 + y**2) - radius)**2 + z**2) - thickness / 2

    # global n
    # if 0 <= n < 100:
    #     print("{")
    #     print(f"{{{int(abs(x))}, {fraction(x, scale)}, {0 if x >= 0 else 1}}},")
    #     print(f"{{{int(abs(y))}, {fraction(y, scale)}, {0 if y >= 0 else 1}}},")
    #     print(f"{{{int(abs(z))}, {fraction(z, scale)}, {0 if z >= 0 else 1}}},")
    #     print(f"{{{int(abs(res))}, {fraction(res, scale)}, {0 if res >= 0 else 1}}},")
    #     print("},")
    # n += 1
    return res


Sdf = typing.Callable[[float, float, float], float]
def normal(sdf: Sdf, x: float, y: float, z: float) -> tuple[float, float, float]:
    ε = 0.001
    n_x = sdf(x + ε, y, z) - sdf(x - ε, y, z)
    n_y = sdf(x, y + ε, z) - sdf(x, y - ε, z)
    n_z = sdf(x, y, z + ε) - sdf(x, y, z - ε)
    norm = math.sqrt(n_x**2 + n_y**2 + n_z**2)
    res = (n_x / norm, n_y / norm, n_z / norm)

    # global n
    # if 0 <= n < 100:
    #     print("{")
    #     print(f"{{{int(abs(x))}, {fraction(x, scale)}, {0 if x >= 0 else 1}}},")
    #     print(f"{{{int(abs(y))}, {fraction(y, scale)}, {0 if y >= 0 else 1}}},")
    #     print(f"{{{int(abs(z))}, {fraction(z, scale)}, {0 if z >= 0 else 1}}},")
    #     print(f"{{{int(abs(res[0]))}, {fraction(res[0], scale)}, {0 if res[0] >= 0 else 1}}},")
    #     print(f"{{{int(abs(res[1]))}, {fraction(res[1], scale)}, {0 if res[1] >= 0 else 1}}},")
    #     print(f"{{{int(abs(res[2]))}, {fraction(res[2], scale)}, {0 if res[2] >= 0 else 1}}},")
    #     print("},")
    # n += 1
    return res

def sample(x: float, y: float) -> str:
    z = -10
    for _step in range(30):
        θ = time.time() * 2
        t_x = x * math.cos(θ) - z * math.sin(θ)
        t_z = x * math.sin(θ) + z * math.cos(θ)
        d = donut(t_x, y, t_z)
        if d <= 0.01:
            _, nt_y, nt_z = normal(donut, t_x, y, t_z)
            is_lit = nt_y < -0.15
            is_frosted = nt_z < -0.5

            if is_frosted:
                return '@' if is_lit else '#'
            else:
                return '=' if is_lit else '.'
        else:
            z += d
    return ' '

while True:
    frame_chars = []
    for y in range(20):
        for x in range(80):
            remapped_x = x / 80 * 2 - 1
            remapped_y = (y / 20 * 2 - 1) * (2 * 20/80)
            frame_chars.append(sample(remapped_x, remapped_y))
        frame_chars.append('\n')
    print('\033[2J' + ''.join(frame_chars))
    time.sleep(1/30)
