// Material Design 2 palette generator (50–900 plus complementary / analogous /
// triadic). Port of Google's published algorithm as extracted by
// edelstone/material-palette-generator (MIT).

#include "ui/MaterialPalette.h"

#include <QtGlobal>

#include <cmath>

namespace gazer {
namespace MaterialPalette {
namespace {

constexpr double kEps = 1.0 / 65536.0;
constexpr double kPi = 3.14159265358979323846;

struct Rgb {
    double r = 0;
    double g = 0;
    double b = 0;
};

struct Hsl {
    double h = 0;
    double s = 0;
    double l = 0;
};

struct Lab {
    double l = 0;
    double a = 0;
    double b = 0;
};

struct Lch {
    double l = 0;
    double c = 0;
    double h = 0;
};

double clamp01(double v)
{
    return qBound(0.0, v, 1.0);
}

int round2(double n)
{
    return int(std::floor(n + 0.5));
}

Rgb fromQColor(const QColor& c)
{
    Rgb o;
    o.r = c.redF();
    o.g = c.greenF();
    o.b = c.blueF();
    return o;
}

QColor toQColor(const Rgb& c)
{
    return QColor(qBound(0, round2(255.0 * clamp01(c.r)), 255),
                  qBound(0, round2(255.0 * clamp01(c.g)), 255),
                  qBound(0, round2(255.0 * clamp01(c.b)), 255));
}

Hsl rgbToHsl(const Rgb& color)
{
    const double r = color.r;
    const double g = color.g;
    const double b = color.b;
    const double maxV = qMax(r, qMax(g, b));
    const double minV = qMin(r, qMin(g, b));
    const double delta = maxV - minV;
    Hsl out;
    out.l = clamp01(0.5 * (maxV + minV));
    if (delta > kEps) {
        if (maxV == r) {
            out.h = 60.0 * ((g - b) / delta);
        } else if (maxV == g) {
            out.h = 60.0 * ((b - r) / delta) + 120.0;
        } else {
            out.h = 60.0 * ((r - g) / delta) + 240.0;
        }
        if (out.l > 0.0 && out.l <= 0.5) {
            out.s = clamp01(delta / (2.0 * out.l));
        } else {
            out.s = clamp01(delta / (2.0 - 2.0 * out.l));
        }
    }
    out.h = std::fmod(double(round2(out.h + 360.0)), 360.0);
    if (out.h < 0.0) {
        out.h += 360.0;
    }
    return out;
}

Rgb hsvChromaMatchToRgb(double hue, double chroma, double match)
{
    Rgb o{match, match, match};
    hue = std::fmod(hue, 360.0) / 60.0;
    if (hue < 0.0) {
        hue += 6.0;
    }
    const double x = chroma * (1.0 - std::abs(std::fmod(hue, 2.0) - 1.0));
    const int floorHue = int(hue);
    if (floorHue == 0) {
        o.r += chroma;
        o.g += x;
    } else if (floorHue == 1) {
        o.r += x;
        o.g += chroma;
    } else if (floorHue == 2) {
        o.g += chroma;
        o.b += x;
    } else if (floorHue == 3) {
        o.g += x;
        o.b += chroma;
    } else if (floorHue == 4) {
        o.r += x;
        o.b += chroma;
    } else {
        o.r += chroma;
        o.b += x;
    }
    return o;
}

Rgb hslToRgb(const Hsl& color)
{
    const double chroma = (1.0 - std::abs(2.0 * color.l - 1.0)) * color.s;
    return hsvChromaMatchToRgb(color.h, chroma, std::max(0.0, color.l - chroma / 2.0));
}

Rgb rotateRgb(const Rgb& color, double angle)
{
    Hsl hsl = rgbToHsl(color);
    hsl.h = std::fmod(hsl.h + angle + 360.0, 360.0);
    if (hsl.h < 0.0) {
        hsl.h += 360.0;
    }
    return hslToRgb(hsl);
}

double srgbToLinear(double v)
{
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double v)
{
    return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

double xyzToLabComponent(double v)
{
    const double b = 6.0 / 29.0;
    if (v > b * b * b) {
        return std::cbrt(v);
    }
    return v / (3.0 * b * b) + 4.0 / 29.0;
}

double labToXyzComponent(double v)
{
    const double b = 6.0 / 29.0;
    if (v > b) {
        return v * v * v;
    }
    return 3.0 * b * b * (v - 4.0 / 29.0);
}

Lab rgbToLab(const Rgb& color)
{
    const double r = srgbToLinear(color.r);
    const double g = srgbToLinear(color.g);
    const double b = srgbToLinear(color.b);
    const double y = 0.2126729 * r + 0.7151522 * g + 0.072175 * b;
    Lab out;
    out.l = 116.0 * xyzToLabComponent(y) - 16.0;
    out.a = 500.0
            * (xyzToLabComponent((0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047)
               - xyzToLabComponent(y));
    out.b = 200.0
            * (xyzToLabComponent(y)
               - xyzToLabComponent((0.0193339 * r + 0.119192 * g + 0.9503041 * b) / 1.08883));
    return out;
}

Lch labToLch(const Lab& color)
{
    Lch out;
    out.l = color.l;
    out.c = std::sqrt(color.a * color.a + color.b * color.b);
    out.h = std::fmod(std::atan2(color.b, color.a) * 180.0 / kPi + 360.0, 360.0);
    if (out.h < 0.0) {
        out.h += 360.0;
    }
    return out;
}

double cartesianToHue(double y, double x)
{
    if (std::abs(y) < 1e-4 && std::abs(x) < 1e-4) {
        return 0.0;
    }
    const double angle = std::atan2(y, x) * 180.0 / kPi;
    return angle >= 0.0 ? angle : angle + 360.0;
}

Rgb labToRgb(const Lab& color)
{
    const double g = (color.l + 16.0) / 116.0;
    const double x = 0.95047 * labToXyzComponent(g + color.a / 500.0);
    const double y = labToXyzComponent(g);
    const double z = 1.08883 * labToXyzComponent(g - color.b / 200.0);
    Rgb out;
    out.r = clamp01(linearToSrgb(3.2404542 * x - 1.5371385 * y - 0.4985314 * z));
    out.g = clamp01(linearToSrgb(-0.969266 * x + 1.8760108 * y + 0.041556 * z));
    out.b = clamp01(linearToSrgb(0.0556434 * x - 0.2040259 * y + 1.0572252 * z));
    return out;
}

constexpr int kGoldenCount = 19;

constexpr Lab kGolden[kGoldenCount][kShadeCount] =
    {
        {
            {94.67497003305085, 7.266715066863771, 1.000743882272359},
            {86.7897416761699, 18.37073676165801, 4.23637133971424},
            {72.09391628325611, 31.7948058298117, 13.2972443996896},
            {61.79353370051851, 44.12949816376454, 20.72147732679961},
            {57.19419539894957, 59.6450006197361, 34.99983001294019},
            {55.60395107186137, 66.01287384845483, 47.67169313982772},
            {51.66348502954747, 64.74877850206251, 43.24487669485529},
            {47.09455666350969, 62.29836039074277, 40.67775424698388},
            {43.77122063388739, 60.28633509183384, 40.31444686692952},
            {39.55518707800739, 58.70368135538997, 41.66495027798629},
        },
        {
            {92.68053776327665, 9.515385232804263, -0.8994072969754852},
            {81.86756643628922, 25.05688089723257, -1.947523511539062},
            {70.90987389545768, 42.21705257720526, -1.095154624057959},
            {61.08140805216186, 58.8712333075872, 2.100876480462643},
            {54.97970219986448, 68.56530938366889, 7.327430728560569},
            {50.87225034074918, 74.60459195925529, 15.35357625689607},
            {47.27738650144558, 70.77855776427805, 11.70434273264508},
            {42.58424189486517, 65.5411953138309, 7.595596439803797},
            {37.97749240725484, 60.74362621842075, 2.984712495145347},
            {29.6992900348496, 51.90485023721311, -4.830186634107636},
        },
        {
            {92.4362655169016, 7.542927467702299, -6.039842848605881},
            {81.07399776904751, 19.56387021780504, -15.71962549198604},
            {68.71394717711831, 33.79992812490556, -26.49539972339321},
            {56.5961612262363, 47.5856631835152, -36.48081660541092},
            {48.00279121762443, 57.30866443934879, -43.2561127152548},
            {40.66211534692161, 64.01910773818436, -48.05930162591041},
            {37.69070220899219, 61.13762767732481, -49.38480327424303},
            {33.56291870731981, 57.6373812392541, -51.39557249855828},
            {29.86539131423451, 54.29737439901333, -52.6601973712463},
            {23.16724235420436, 48.51764437280498, -55.16267949015293},
        },
        {
            {92.49103426017201, 4.712320025752947, -6.532868071709763},
            {81.24668319505597, 11.50642734909485, -16.66660063724537},
            {68.61488216554629, 20.39532905198282, -28.52201885171542},
            {55.60369793053023, 30.933537768905, -41.16439122358484},
            {45.83456619096943, 39.28806272235674, -50.52332205277263},
            {36.60862022935866, 47.29686002828143, -59.11176658618685},
            {34.18979123756262, 46.60426065139123, -59.53961627676729},
            {30.52713367338361, 46.01498224754519, -60.19975052509064},
            {27.44585524877222, 44.96180431854785, -60.46395810756433},
            {21.98627670328218, 44.29296076245473, -60.93653655172098},
        },
        {
            {92.86314411983918, 1.531814706106194, -6.025243528950552},
            {81.8348073705298, 4.460934955458907, -15.87356100973614},
            {69.7796913795672, 7.904365255891276, -26.3170846346932},
            {57.48786519938736, 12.68101950482253, -37.23202012914528},
            {47.74592578811101, 18.52079930245237, -46.47540679000397},
            {38.3344036144554, 25.57700668170812, -55.28224153299287},
            {35.15116453901552, 26.23181208038117, -54.53700978785404},
            {31.08042998800796, 27.07394930110124, -53.97505274579958},
            {27.02667208045492, 28.16526642755898, -53.28987325482218},
            {19.75120158792168, 30.60784576895101, -52.13866519297474},
        },
        {
            {94.70682457348717, -2.835484735987326, -6.978044694792707},
            {86.8839842970016, -5.16908728759552, -17.88561192754956},
            {79.0451532401558, -6.817753527015746, -28.96853749043218},
            {71.15083697242613, -5.994763756850707, -39.72549451158927},
            {65.48106058907833, -2.735745792537936, -48.15471238926561},
            {60.43009440850862, 2.079928897321559, -55.10935847069616},
            {55.62267676922188, 4.998684384486918, -55.02164729429915},
            {49.27006645904875, 8.470398370314381, -54.49479683845755},
            {43.16828856394358, 11.96848307614384, -53.97256737797797},
            {32.17757793894193, 18.96054990229354, -53.45146365049088},
        },
        {
            {95.35713467762652, -4.797149155388203, -6.550002550504308},
            {88.27942649540043, -10.83600661458389, -16.35936182194038},
            {81.10009044900976, -15.32305452298172, -26.41912119132095},
            {74.44713958259777, -16.66443262536255, -35.19702686900037},
            {69.87836465637318, -14.29151533205469, -41.82743032975517},
            {65.68851259178913, -9.612635721963692, -47.34091616039191},
            {60.88357994308973, -7.252819027184943, -46.67753731595634},
            {54.26166495426166, -3.814183689790807, -45.97939475762498},
            {48.10661895072673, -1.378998784464347, -44.34466750206778},
            {36.34401147057282, 5.067812404713545, -43.11786257561915},
        },
        {
            {95.69295154599753, -6.898716127301141, -3.994284229654421},
            {89.52842524059004, -16.41239828960173, -9.260466069266693},
            {83.32031214655748, -24.83036840728098, -14.5686735833046},
            {77.35338313752958, -30.2017085722151, -18.92358284721101},
            {73.45322093857781, -31.88590390189383, -21.13045999251369},
            {69.97638465064783, -30.67985032454795, -23.18668566113671},
            {64.44491716553777, -29.08337434584457, -21.15493576915621},
            {56.99816432961103, -27.31081477279451, -17.86988815767443},
            {49.75464182255671, -25.33538350369424, -15.02472259166279},
            {36.52725894264432, -22.12964174419452, -9.176159146894303},
        },
        {
            {94.18453941589918, -6.08351703428972, -1.548891605116198},
            {85.68177077414457, -15.33317944029861, -2.851982576147605},
            {76.85067847190405, -24.84405917318971, -3.875078513219266},
            {68.02762242570138, -32.56686115412072, -4.015231084407134},
            {61.66725730452546, -36.06752603289354, -3.473404640175382},
            {55.67310397390196, -36.66069960626328, -2.125617915169653},
            {51.05914949519772, -34.65019160301408, -1.391048430043251},
            {45.2690810192184, -32.13244775422941, -0.4526371852697775},
            {39.36899076059384, -29.25264468583161, -0.03562564673170732},
            {28.58363043701477, -24.58546551613641, 1.803740216249239},
        },
        {
            {95.30530183565223, -6.430415645739263, 4.292950594459599},
            {88.49014579152143, -15.23147744952702, 10.84826117768314},
            {81.22616870575376, -24.99388616855158, 18.14469680333088},
            {74.30361721558802, -35.56088696067356, 26.78151525190773},
            {69.0430995277442, -42.61556126595995, 33.17109563126665},
            {63.97742181407293, -48.54292673319982, 39.73241526342939},
            {58.77796085346137, -46.1153692478013, 37.83891074522558},
            {52.41108688974904, -43.21761792485762, 35.62250659009424},
            {46.2813873076426, -40.25816227675361, 33.32343229338761},
            {34.68565530581451, -34.75343878510312, 28.86673903435977},
        },
        {
            {96.70518169355954, -4.929987845095463, 6.397084523168894},
            {91.66416061199438, -12.05703204194569, 16.05460457927514},
            {86.2244395865449, -19.61364683408062, 26.38490642345424},
            {80.83404879636919, -27.08017184075689, 37.37849374202133},
            {76.79543725108964, -32.76659719736752, 45.91219057244444},
            {72.90025297028019, -37.54913922392738, 53.51959496103027},
            {67.21532310272079, -36.56304870773486, 50.49629051268894},
            {59.91051142210195, -35.77011466063357, 46.56465847976187},
            {52.51015841084511, -34.47903440699235, 42.20723868724268},
            {39.41191983353878, -32.80460974352642, 35.25549058563001},
        },
        {
            {97.99506057883428, -4.059632482741494, 9.355797602381521},
            {94.80926235976536, -9.237091467352855, 23.23065006482499},
            {91.85205843526167, -15.05391732701111, 38.86115182206598},
            {88.75812142080242, -19.5429004001641, 53.71785675783709},
            {86.27404180729515, -22.1739928911216, 63.97863906523251},
            {84.20566835376492, -24.27064352098934, 72.79624067033038},
            {78.27915100603997, -21.1818500564025, 68.82763412297965},
            {70.82385811892824, -17.78814893252567, 64.00327817988128},
            {62.93686701286803, -13.6974121116849, 58.51300050928783},
            {49.49861088145254, -6.485230564384715, 49.67432722833751},
        },
        {
            {98.93885129752759, -3.009847028854318, 10.76573683379001},
            {97.22689784824074, -6.174599368734491, 26.22932417355146},
            {95.58092947828766, -8.907132848473886, 43.56297291446567},
            {94.09009515702486, -10.50962894271074, 60.20019514231188},
            {93.06546746683087, -11.00855847601301, 71.76500826005477},
            {92.12975017760128, -10.8300230948683, 80.9090559640089},
            {87.12188349168609, -2.376430009923935, 78.14868195373407},
            {80.96200442419905, 8.849333792729064, 75.05050700092679},
            {75.00342770718086, 20.34017356687928, 72.24841925958934},
            {65.48207757431567, 39.64706497047609, 68.34872841768654},
        },
        {
            {97.5642392074337, -1.445525639405032, 11.88125431629767},
            {93.67057953749456, -1.869309686207243, 30.02888670415651},
            {89.94571492804107, -1.022450381476969, 49.64954236164228},
            {86.71009164153801, 1.049606639642819, 68.77377342409739},
            {83.78773993319211, 5.248231820098425, 78.92920457852716},
            {81.52191382080228, 9.403655370707199, 82.69257112982746},
            {78.17240973804697, 16.62851288653189, 81.09358318806208},
            {73.80899654381052, 26.53614315250874, 78.21754052181723},
            {70.1134511665764, 35.3007623359744, 75.87510992138593},
            {63.86460405565717, 50.94648214505959, 72.17815682124423},
        },
        {
            {96.30459517801387, 0.923151172282477, 10.59843944608307},
            {90.68320082865087, 4.103774964681062, 26.48579372191613},
            {85.00055287186233, 9.047181758866651, 44.51407622580792},
            {79.42428495742953, 16.45261072443988, 62.08721739074201},
            {75.47792699289774, 23.39574292845187, 72.64347611236501},
            {72.04246561548388, 30.6819210123821, 77.08579298904603},
            {68.94724338946975, 35.22014778433863, 74.88425044595111},
            {64.83017495535229, 40.91200730099703, 71.9596053545428},
            {60.8534207471871, 46.41483590510681, 69.18061963415211},
            {54.77571742962287, 55.28275101936003, 65.10193403547922},
        },
        {
            {93.69219844671957, 5.763979334358293, 3.170016279646903},
            {86.04629434276428, 15.75084380395819, 14.82847692709099},
            {77.54010042938336, 27.90113842540043, 25.99645229289065},
            {69.74095456707857, 41.14487377552256, 39.44332017890002},
            {64.37085344539341, 51.89037962044357, 50.81312471046415},
            {60.06780837277435, 61.65258736118817, 61.54771829165221},
            {57.28707915232363, 60.3250664308812, 60.07341536376447},
            {53.81005261629385, 58.36760943780162, 58.19586806694884},
            {50.30135240510587, 56.40104898089937, 55.92414199240434},
            {43.86477994548343, 52.97088770391073, 52.30067989225532},
        },
        {
            {93.29864888069987, 0.9915456090475727, 1.442353076378411},
            {82.80884359004081, 3.116221903342209, 3.352305945146306},
            {70.95493047668185, 5.469742193344784, 5.449009494553492},
            {58.71293461910307, 7.990991075363385, 8.352488495367627},
            {49.1502085528759, 10.5709849810004, 10.83144015119792},
            {39.63200151837749, 13.13888196162724, 13.53157471151188},
            {35.60099668201575, 12.40352847757295, 12.10432183902449},
            {30.08427126575995, 11.31714814987808, 10.54748430429622},
            {24.55501469641658, 10.81661331678246, 8.506555306791984},
            {18.35055226514404, 10.22572555033877, 7.058582769882571},
        },
        {
            {98.27202740980219, -1.641839364463493e-05, 6.567357457853973e-06},
            {96.53749336548567, -1.616917905122861e-05, 6.467671598286984e-06},
            {94.0978378987781, -1.581865383126768e-05, 6.327461532507073e-06},
            {89.17728373493613, -1.511167768697419e-05, 6.044671074789676e-06},
            {76.61119902231323, -1.330620591488696e-05, 5.322482343750323e-06},
            {65.11424774127516, -1.165434515559838e-05, 4.661738062239351e-06},
            {49.23898962082806, -9.373417431124409e-06, 3.749366972449764e-06},
            {41.14266843804848, -8.210152946386273e-06, 3.28406118965674e-06},
            {27.9748572060037, -6.318226192236764e-06, 2.527290476894706e-06},
            {12.74001133130272, -4.129311698131133e-06, 1.651724679252453e-06},
        },
        {
            {94.27665212516236, -0.637571046109342, -1.313515378996688},
            {85.77788001492097, -2.277781108451282, -3.017775841615156},
            {76.12296325015231, -3.401502988883809, -5.16867892977908},
            {66.16340108908365, -4.819627183079045, -7.520697631614404},
            {58.35752478513645, -5.71950891008921, -9.165988916613488},
            {50.70748082202715, -6.837992965799455, -10.95605511240936},
            {44.85917867647632, -6.411990559239578, -9.745119828787651},
            {36.92458930566504, -5.319878610845596, -8.341943474561553},
            {29.11533478463762, -4.168907828645069, -6.86299621999733},
            {19.95833845079991, -3.311672145318662, -5.448614210473679},
        },
    };

constexpr double kLightnessWeights[kShadeCount] = {2.048875457, 5.124792061, 8.751659557, 12.07628774, 13.91449542, 15.92738893, 15.46585818, 15.09779227, 15.13738673, 15.09818372};

constexpr double kChromaWeights[kShadeCount] = {1.762442714, 4.213532634, 7.395827458, 11.07174158, 13.89634504, 16.37591477, 16.27071136, 16.54160806, 17.35916727, 19.88410864};

struct Nearest {
    int palette = 0;
    int shade = 0;
};

Nearest findNearest(const Lab& color)
{
    double best = 1e300;
    Nearest out;
    for (int g = 0; g < kGoldenCount; ++g) {
        for (int h = 0; h < kShadeCount; ++h) {
            if (best <= 0.0) {
                break;
            }
            const Lab k = kGolden[g][h];
            const double l = (k.l + color.l) / 2.0;
            const double m0 = std::sqrt(k.a * k.a + k.b * k.b);
            const double q0 = std::sqrt(color.a * color.a + color.b * color.b);
            double t = (m0 + q0) / 2.0;
            t = 0.5 * (1.0 - std::sqrt(std::pow(t, 7.0) / (std::pow(t, 7.0) + std::pow(25.0, 7.0))));
            const double n = k.a * (1.0 + t);
            const double r = color.a * (1.0 + t);
            const double N0 = std::sqrt(n * n + k.b * k.b);
            const double H0 = std::sqrt(r * r + color.b * color.b);
            const double dC = H0 - N0;
            const double ja = (N0 + H0) / 2.0;
            const double hn = cartesianToHue(k.b, n);
            const double hr = cartesianToHue(color.b, r);
            double deltaHue = 0.0;
            if (std::abs(m0) >= 1e-4 && std::abs(q0) >= 1e-4) {
                if (std::abs(hr - hn) <= 180.0) {
                    deltaHue = hr - hn;
                } else if (hr <= hn) {
                    deltaHue = hr - hn + 360.0;
                } else {
                    deltaHue = hr - hn - 360.0;
                }
            }
            const double dH =
                2.0 * std::sqrt(N0 * H0) * std::sin((deltaHue / 2.0) * kPi / 180.0);
            double m = 0.0;
            if (std::abs(m0) >= 1e-4 && std::abs(q0) >= 1e-4) {
                if (std::abs(hr - hn) <= 180.0) {
                    m = (hn + hr) / 2.0;
                } else if ((hn + hr) < 360.0) {
                    m = (hn + hr + 360.0) / 2.0;
                } else {
                    m = (hn + hr - 360.0) / 2.0;
                }
            }
            const double q = 1.0 + 0.045 * ja;
            const double H =
                1.0
                + 0.015 * ja
                      * (1.0 - 0.17 * std::cos((m - 30.0) * kPi / 180.0)
                         + 0.24 * std::cos((2.0 * m) * kPi / 180.0)
                         + 0.32 * std::cos((3.0 * m + 6.0) * kPi / 180.0)
                         - 0.2 * std::cos((4.0 * m - 63.0) * kPi / 180.0));
            const double dL =
                (color.l - k.l)
                / (1.0 + 0.015 * ((l - 50.0) * (l - 50.0))
                                 / std::sqrt(20.0 + (l - 50.0) * (l - 50.0)));
            const double term1 = dL * dL;
            const double term2 = (dC / q) * (dC / q);
            const double term3 = (dH / H) * (dH / H);
            const double cross =
                (dC / q)
                * std::sqrt(std::pow(ja, 7.0) / (std::pow(ja, 7.0) + std::pow(25.0, 7.0)))
                * std::sin((60.0 * std::exp(-std::pow((m - 275.0) / 25.0, 2.0))) * kPi / 180.0)
                * -2.0 * (dH / H);
            const double dist = std::sqrt(term1 + term2 + term3 + cross);
            if (dist < best) {
                best = dist;
                out.palette = g;
                out.shade = h;
            }
        }
    }
    return out;
}

void fillShades(const Rgb& color, QColor* out)
{
    const Lab c = rgbToLab(color);
    const Nearest near = findNearest(c);
    const Lab* row = kGolden[near.palette];
    const Lch g = labToLch(row[near.shade]);
    const Lch h = labToLch(c);
    const bool lowChroma = labToLch(row[5]).c < 30.0;
    const double l = g.l - h.l;
    const double m = g.c - h.c;
    const double q = g.h - h.h;
    const double t = kLightnessWeights[near.shade];
    const double n = kChromaWeights[near.shade];
    double r = 100.0;
    for (int i = 0; i < kShadeCount; ++i) {
        if (near.shade == i) {
            r = std::max(h.l - 1.7, 0.0);
            out[i] = toQColor(color);
            continue;
        }
        const Lch bLch = labToLch(row[i]);
        double dVal = bLch.l - kLightnessWeights[i] / t * l;
        dVal = std::min(dVal, r);
        Lch cLch;
        cLch.l = qBound(0.0, dVal, 100.0);
        const double chromaScale = lowChroma ? 1.0 : std::min(kChromaWeights[i] / n, 1.25);
        cLch.c = std::max(0.0, bLch.c - m * chromaScale);
        cLch.h = std::fmod(bLch.h - q + 360.0, 360.0);
        if (cLch.h < 0.0) {
            cLch.h += 360.0;
        }
        r = std::max(cLch.l - 1.7, 0.0);
        const double angle = cLch.h * kPi / 180.0;
        Lab cLab;
        cLab.l = cLch.l;
        cLab.a = cLch.c * std::cos(angle);
        cLab.b = cLch.c * std::sin(angle);
        out[i] = toQColor(labToRgb(cLab));
    }
}

} // namespace

Palettes generate(const QColor& sourceIn)
{
    const QColor source = sourceIn.isValid() ? sourceIn : QColor(0xE9, 0x1E, 0x63);
    const Rgb rgb = fromQColor(source);
    Palettes out;
    fillShades(rgb, out.primary);
    fillShades(rotateRgb(rgb, 180.0), out.complementary);
    fillShades(rotateRgb(rgb, -30.0), out.analogous1);
    fillShades(rotateRgb(rgb, 30.0), out.analogous2);
    fillShades(rotateRgb(rgb, 60.0), out.triadic1);
    fillShades(rotateRgb(rgb, 120.0), out.triadic2);
    return out;
}

const QColor* familyColors(const Palettes& palettes, Family family)
{
    switch (family) {
    case Family::Primary:
        return palettes.primary;
    case Family::Complementary:
        return palettes.complementary;
    case Family::Analogous1:
        return palettes.analogous1;
    case Family::Analogous2:
        return palettes.analogous2;
    case Family::Triadic1:
        return palettes.triadic1;
    case Family::Triadic2:
        return palettes.triadic2;
    }
    return palettes.primary;
}

QColor shade(const Palettes& palettes, Family family, int index)
{
    const int i = qBound(0, index, kShadeCount - 1);
    return familyColors(palettes, family)[i];
}

const char* familyId(Family family)
{
    switch (family) {
    case Family::Primary:
        return "primary";
    case Family::Complementary:
        return "complementary";
    case Family::Analogous1:
        return "analogous1";
    case Family::Analogous2:
        return "analogous2";
    case Family::Triadic1:
        return "triadic1";
    case Family::Triadic2:
        return "triadic2";
    }
    return "primary";
}

const char* familyLabel(Family family)
{
    switch (family) {
    case Family::Primary:
        return "Primary";
    case Family::Complementary:
        return "Complementary";
    case Family::Analogous1:
        return "Analogous";
    case Family::Analogous2:
        return "Analogous";
    case Family::Triadic1:
        return "Tertiary";
    case Family::Triadic2:
        return "Tertiary";
    }
    return "Primary";
}

bool parseFamily(const QString& id, Family* family)
{
    if (!family) {
        return false;
    }
    if (id == QLatin1String("primary")) {
        *family = Family::Primary;
        return true;
    }
    if (id == QLatin1String("complementary")) {
        *family = Family::Complementary;
        return true;
    }
    if (id == QLatin1String("analogous") || id == QLatin1String("analogous1")
        || id == QLatin1String("analogous-1")) {
        *family = Family::Analogous1;
        return true;
    }
    if (id == QLatin1String("analogous2") || id == QLatin1String("analogous-2")) {
        *family = Family::Analogous2;
        return true;
    }
    if (id == QLatin1String("tertiary1") || id == QLatin1String("triadic1")
        || id == QLatin1String("triadic-1")) {
        *family = Family::Triadic1;
        return true;
    }
    if (id == QLatin1String("tertiary") || id == QLatin1String("tertiary2")
        || id == QLatin1String("triadic2") || id == QLatin1String("triadic-2")) {
        *family = Family::Triadic2;
        return true;
    }
    return false;
}

int shadeIndexForWeight(int weight)
{
    for (int i = 0; i < kShadeCount; ++i) {
        if (kShades[i] == weight) {
            return i;
        }
    }
    return 5;
}

bool sameRgb(const QColor& a, const QColor& b)
{
    if (!a.isValid() || !b.isValid()) {
        return false;
    }
    return a.red() == b.red() && a.green() == b.green() && a.blue() == b.blue();
}

} // namespace MaterialPalette
} // namespace gazer
