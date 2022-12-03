
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