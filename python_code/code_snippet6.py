
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
    'P256',
    115792089210356248762697446949407573530086143415290314195533631308867097853951,
    -3,
    41058363725152142129326129780047268409114441015993725554835256314039467401291,
    115792089210356248762697446949407573529996955224135760342422259061068512044369,
    48439561293906451759052585252797914202762949526041747995844080717082404635286,
    36134250956749795798585127919587881956611106672985015071877198253568414405109,
    b'\x2A\x86\x48\xCE\x3D\x03\x01\x07'
)
P384 = Curve(
    'P384',
    int('39402006196394479212279040100143613805079739270465446667948293404245721771496870329047266'
        '088258938001861606973112319'),
    -3,
    int('27580193559959705877849011840389048093056905856361568521428707301988689241309860865136260'
        '764883745107765439761230575'),
    int('39402006196394479212279040100143613805079739270465446667946905279627659399113263569398956'
        '308152294913554433653942643'),
    int('26247035095799689268623156744566981891852923491109213387815615900925518854738050089022388'
        '053975719786650872476732087'),
    int('83257109614890299855467512895201081792878530488613155947092059024805031998844192244386437'
        '60392947333078086511627871'),
    b'\x2B\x81\x04\x00\x22'
)
P521 = Curve(
    'P521',
    int('68647976601306097149819007990813932172694353001433054093944634591855431833976560521225596'
        '40661454554977296311391480858037121987999716643812574028291115057151'),
    -3,
    int('10938490380737342745111123907668055699362075989516837489945863944959531161507350160137087'
        '37573759623248592132296706313309438452531591012912142327488478985984'),
    int('68647976601306097149819007990813932172694353001433054093944634591855431833976553942450577'
        '46333217197532963996371363321113864768612440380340372808892707005449'),
    int('26617408020502170632287687167233609607298591687569731477066713684188029449964278084915450'
        '80627771902352094241225065558662157113545570916814161637315895999846'),
    int('37571800257700204635455072244911836035944551347697624866945677796155444774405563166912344'
        '05012945539562144444537289428522585666729196580810124344277578376784'),
    b'\x2B\x81\x04\x00\x23'
)
W25519 = Curve(
    'W25519',
    0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffed,
    0x2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa984914a144,
    0x7b425ed097b425ed097b425ed097b425ed097b425ed097b4260b5e9c7710c864,
    0x1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed,
    0x2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaad245a,
    0x5f51e65e475f794b1fe122d388b72eb36dc2b28192839e4dd6163a5d81312c14,
)
W448 = Curve(
    'W448',
    int('72683872429560689054932380788800453435364136068731806028149019918061232816673077268639638'
        '3698676545930088884461843637361053498018365439'),
    int('48455914953040459369954920525866968956909424045821204018766013278707488544448718179093092'
        '2465784363953392589641229091574035657199637535'),
    int('26919952751689144094419400292148316087171902247678446677092229599281938080249287877273940'
        '1369880202196329216467349495319191685664513904'),
    int('18170968107390172263733095197200113358841034017182951507037254979514600396153958571619575'
        '5291692375963310293709091662304773755859649779'),
    int('48455914953040459369954920525866968956909424045821204018766013278707488544448718179093092'
        '2465784363953392589641229091574035665345629073'),
    int('35529392678556817526412750206378333480897639938771427183188089843516908878696741000293267'
        '3765864550910142774147268105838985595290606362'),
)

# see http://www.secg.org/sec2-v2.pdf for params
secp192k1 = Curve(
    'secp192k1',
    0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFEE37,
    0x0,
    0x3,
    0xFFFFFFFFFFFFFFFFFFFFFFFE26F2FC170F69466A74DEFD8D,
    0xDB4FF10EC057E9AE26B07D0280B7F4341DA5D1B1EAE06C7D,
    0x9B2F2F6D9C5628A7844163D015BE86344082AA88D95E2F9D,
    b'\x2B\x81\x04\x00\x1F'
)

secp224k1 = Curve(
    'secp224k1',
    0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFE56D,
    0x0,
    0x5,
    0x10000000000000000000000000001DCE8D2EC6184CAF0A971769FB1F7,