
class Curve:
    """Representation of an elliptic curve.

    Defines a group for  the arithmetic operations of point addition and scalar multiplication.
    Currently only curves defined via the equation :math:`y^2 \equiv x^3 + ax + b \pmod{p}` are
    supported.

    Attributes:
        |  name (str): The name of the curve
        |  p (int): The value of :math:`p` in the curve equation.
        |  a (int): The value of :math:`a` in the curve equation.
        |  b (int): The value of :math:`b` in the curve equation.
        |  q (int): The order of the base point of the curve.
        |  oid (bytes): The object identifier of the curve.
    """
    _oid_lookup = {}  # a lookup table for getting curve instances by their object identifier

    def __init__(self, name: str, p: int, a: int, b: int, q: int, gx: int, gy: int, oid: bytes = None):
        """Initialize the parameters of an elliptic curve.

        WARNING: Do not generate your own parameters unless you know what you are doing or you could
        generate a curve severely less secure than you think. Even then, consider using a
        standardized curve for the sake of interoperability.

        Currently only curves defined via the equation :math:`y^2 \equiv x^3 + ax + b \pmod{p}` are
        supported.

        Args:
            |  name (string): The name of the curve
            |  p (int): The value of :math:`p` in the curve equation.
            |  a (int): The value of :math:`a` in the curve equation.
            |  b (int): The value of :math:`b` in the curve equation.
            |  q (int): The order of the base point of the curve.
            |  gx (int): The x coordinate of the base point of the curve.
            |  gy (int): The y coordinate of the base point of the curve.
            |  oid (bytes): The object identifier of the curve.
        """
        self.name = name
        self.p = p
        self.a = a
        self.b = b
        self.q = q
        self.gx = gx
        self.gy = gy
        self.oid = oid

        if oid is not None:
            self._oid_lookup[oid] = self

    def __repr__(self) -> str:
        return self.name

    @classmethod
    def get_curve_by_oid(cls, oid: bytes):
        """Get a curve via it's object identifier."""
        return cls._oid_lookup.get(oid, None)

    def is_point_on_curve(self, point: (int, int)) -> bool:
        """ Check if a point lies on this curve.

        The check is done by evaluating the curve equation :math:`y^2 \equiv x^3 + ax + b \pmod{p}`
        at the given point :math:`(x,y)` with this curve's domain parameters :math:`(a, b, p)`. If
        the congruence holds, then the point lies on this curve.

        Args:
            point (long, long): A tuple representing the point :math:`P` as an :math:`(x, y)` coordinate
            pair.

        Returns:
            bool: :code:`True` if the point lies on this curve, otherwise :code:`False`.
        """
        x, y, = point
        left = y * y
        right = (x * x * x) + (self.a * x) + self.b
        return (left - right) % self.p == 0

    def evaluate(self, x: int) -> int:
        """ Evaluate the elliptic curve polynomial at 'x'

        Args:
            x (int): The position to evaluate the polynomial at

        Returns:
            int: the value of :math:`(x^3 + ax + b) \bmod{p}`
        """
        return (x ** 3 + self.a * x + self.b) % self.p

    @property
    def G(self):
        """The base point of the curve.

        For the purposes of ECDSA this point is multiplied by a private key to obtain the
        corresponding public key. Make a property to avoid cyclic dependency of Point on Curve
        (a point lies on a curve) and Curve on Point (curves have a base point).
        """
        from .point import Point
        return Point(self.gx, self.gy, self)


# see https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-186-draft.pdf
# Setion 4.2.1 "Weierstrass Curves" for params
P192 = Curve(
    'P192',
    6277101735386680763835789423207666416083908700390324961279,
    -3,
    2455155546008943817740293915197451784769108058161191238065,
    6277101735386680763835789423176059013767194773182842284081,
    602046282375688656758213480587526111916698976636884684818,
    174050332293622031404857552280219410364023488927386650641,
    b'\x2A\x86\x48\xCE\x3D\x03\x01\x01'
)
P224 = Curve(
    'P224',
    26959946667150639794667015087019630673557916260026308143510066298881,
    -3,
    18958286285566608000408668544493926415504680968679321075787234672564,
    26959946667150639794667015087019625940457807714424391721682722368061,
    19277929113566293071110308034699488026831934219452440156649784352033,
    19926808758034470970197974370888749184205991990603949537637343198772,
    b'\x2B\x81\x04\x00\x21'
)
P256 = Curve(