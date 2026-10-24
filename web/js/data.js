/* SmartCW trainer content: Koch order, words, abbreviations, Q-codes,
 * prosigns, callsign prefixes and QSO building blocks. */
"use strict";

const KOCH = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X".split("");

const COMMON_WORDS = (
  "THE AND FOR ARE BUT NOT YOU ALL ANY CAN HAD HER WAS ONE OUR OUT DAY GET HAS HIM HIS HOW MAN NEW NOW OLD SEE " +
  "TWO WAY WHO DID ITS LET PUT SAY SHE TOO USE THAT WITH HAVE THIS WILL YOUR FROM THEY KNOW WANT BEEN GOOD MUCH " +
  "SOME TIME VERY WHEN COME HERE JUST LIKE LONG MAKE MANY OVER SUCH TAKE THAN THEM WELL WERE WHAT WORK YEAR BACK " +
  "CALL NAME HOME RIG ANT WIRE BAND LOG CARD COPY KEY TEST RADIO POWER SIGNAL NOISE WATTS FIELD PARK SUMMIT " +
  "SUNNY CLOUDY RAIN SNOW WARM COLD NICE AGAIN PLEASE THANKS FRIEND WEATHER MORNING EVENING NIGHT CONTACT"
).split(" ");

const ABBREVIATIONS = [
  ["ABT", "about"], ["AGN", "again"], ["ANT", "antenna"], ["B4", "before"], ["BK", "break, back to you"],
  ["CFM", "confirm"], ["CL", "closing station"], ["CPY", "copy"], ["CQ", "calling any station"],
  ["CU", "see you"], ["CUL", "see you later"], ["DE", "from, this is"], ["DR", "dear"], ["DX", "distant station"],
  ["ES", "and"], ["FB", "fine business, excellent"], ["FER", "for"], ["GA", "good afternoon, go ahead"],
  ["GE", "good evening"], ["GM", "good morning"], ["GN", "good night"], ["GL", "good luck"], ["HI", "laughter"],
  ["HPE", "hope"], ["HR", "here"], ["HW", "how (do you copy)"], ["NR", "number"], ["OM", "old man, operator"],
  ["OP", "operator"], ["PSE", "please"], ["PWR", "power"], ["R", "roger, received"], ["RIG", "the radio"],
  ["RPT", "report, repeat"], ["RST", "readability strength tone"], ["SIG", "signal"], ["SRI", "sorry"],
  ["TNX", "thanks"], ["TU", "thank you"], ["UR", "your, you are"], ["VY", "very"], ["WX", "weather"],
  ["XYL", "wife"], ["YL", "young lady, female operator"], ["73", "best regards"], ["88", "love and kisses"],
  ["5NN", "599, the contest report"], ["TEMP", "temperature"], ["YRS", "years"], ["AGE", "age"],
  ["LIC", "licensed"], ["EFHW", "end-fed half-wave antenna"], ["QRPP", "very low power"], ["HB", "homebrew"],
];

const QCODES = [
  ["QRL", "is the frequency busy?", "the frequency is busy"],
  ["QRM", "is there interference?", "man-made interference"],
  ["QRN", "are you troubled by static?", "static, atmospheric noise"],
  ["QRO", "shall I increase power?", "increase power"],
  ["QRP", "shall I decrease power?", "low power"],
  ["QRQ", "shall I send faster?", "send faster"],
  ["QRS", "shall I send slower?", "send slower"],
  ["QRT", "shall I stop sending?", "stop sending, closing down"],
  ["QRU", "have you anything for me?", "I have nothing for you"],
  ["QRV", "are you ready?", "I am ready"],
  ["QRX", "when will you call again?", "wait, stand by"],
  ["QRZ", "who is calling me?", "you are being called by"],
  ["QSB", "are my signals fading?", "your signals are fading"],
  ["QSL", "can you acknowledge receipt?", "I acknowledge, confirmation card"],
  ["QSO", "can you communicate with?", "a contact"],
  ["QSY", "shall I change frequency?", "change frequency"],
  ["QTH", "what is your location?", "my location is"],
  ["QRG", "what is my exact frequency?", "your exact frequency is"],
  ["QSK", "can you hear between your signals?", "full break-in"],
  ["QSP", "will you relay?", "I will relay"],
  ["QTR", "what is the exact time?", "the exact time is"],
];

const PROSIGNS = [
  ["<AR>", "+", "end of message"],
  ["<KN>", "(", "go ahead, only the station I am working"],
  ["<SK>", "|", "end of contact"],
  ["<BT>", "=", "break, new paragraph"],
  ["<AS>", "&", "wait"],
  ["<BK>", "^", "break, back to you"],
  ["<HH>", "#", "error, I repeat"],
  ["<SN>", "*", "understood"],
  ["K", "K", "go ahead, any station"],
];

// prefix, weight, towns
const PREFIXES = [
  ["ON", 6, "BRUGGE GENT ANTWERP LEUVEN LIEGE NAMUR HASSELT"], ["PA", 5, "AMSTERDAM UTRECHT ROTTERDAM GRONINGEN"],
  ["PD", 2, "EINDHOVEN ZWOLLE"], ["DL", 8, "BERLIN MUNICH HAMBURG KOELN DRESDEN"], ["DK", 3, "BREMEN KASSEL"],
  ["F", 5, "PARIS LYON LILLE NANTES"], ["G", 5, "LONDON YORK LEEDS BRISTOL"], ["M", 3, "OXFORD DERBY"],
  ["EA", 4, "MADRID VALENCIA SEVILLA"], ["I", 4, "ROMA MILANO TORINO"], ["IK", 2, "BOLOGNA GENOVA"],
  ["K", 6, "OHIO TEXAS MAINE IOWA"], ["W", 6, "BOSTON DENVER ATLANTA"], ["N", 4, "DALLAS SEATTLE"],
  ["VE", 3, "OTTAWA TORONTO"], ["JA", 3, "TOKYO OSAKA"], ["OH", 2, "HELSINKI OULU"], ["SM", 2, "STOCKHOLM UPPSALA"],
  ["OZ", 2, "AARHUS ODENSE"], ["SP", 3, "KRAKOW WARSZAWA"], ["OK", 3, "PRAHA BRNO"], ["HB9", 2, "BERN ZURICH"],
  ["OE", 2, "WIEN GRAZ"], ["LA", 2, "OSLO BERGEN"], ["EI", 1, "DUBLIN CORK"], ["VK", 1, "SYDNEY PERTH"],
  ["S5", 1, "LJUBLJANA"], ["9A", 1, "ZAGREB"], ["YO", 1, "BUCURESTI"], ["LZ", 1, "SOFIA"], ["UA", 2, "MOSKVA"],
  ["CT", 1, "LISBOA PORTO"], ["LY", 1, "VILNIUS"], ["ES", 1, "TALLINN"], ["HA", 1, "BUDAPEST"], ["ZS", 1, "DURBAN"],
];

const NAMES = ("JOHN PETE JAN LUC HANS MIKE BOB TOM ANNA MARIE PAUL MARK DAVE JIM BILL KEES PIET JOS DIRK WIM " +
  "FRANK KARL PIERRE MARCO JOSE ERIK LARS OLE IVAN KEN STEF GERT RUDY ELS SARA").split(" ");
const RIGS = "IC7300 K3 FT991 TS590 X6100 KX2 FTDX10 QCX HOMEBREW IC705 G90".split(" ");
const ANTS = "DIPOLE VERTICAL EFHW YAGI LOOP LONGWIRE HEXBEAM GP".split(" ");
const WX = "SUNNY CLOUDY RAIN SNOW WINDY FOGGY CLEAR".split(" ");

function rnd(n) { return Math.floor(Math.random() * n); }
function pick(a) { return a[rnd(a.length)]; }
function weighted(list) {
  let total = 0;
  for (const x of list) total += x[1];
  let r = Math.random() * total;
  for (const x of list) { r -= x[1]; if (r <= 0) return x; }
  return list[list.length - 1];
}

function randomCall(opts = {}) {
  const p = weighted(PREFIXES);
  const L = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  let call = p[0];
  if (!/\d$/.test(call)) call += String(rnd(10));
  const len = 1 + rnd(3) + (Math.random() < 0.5 ? 1 : 0);
  for (let i = 0; i < Math.min(len, 3); i++) call += L[rnd(26)];
  if (opts.portable !== false && Math.random() < 0.08) call += "/P";
  return { call, prefix: p[0], qth: pick(p[2].split(" ")) };
}

function randomRst() {
  return `${4 + rnd(2)}${5 + rnd(5)}9`;
}

// A made-up station on the other side of the QSO.
function makeDx() {
  const c = randomCall({ portable: false });
  return {
    call: c.call, qth: c.qth, name: pick(NAMES), rst: randomRst(), rig: pick(RIGS), ant: pick(ANTS),
    wx: pick(WX), temp: String(rnd(28)), pwr: pick(["5", "10", "50", "100"]), nr: String(1 + rnd(400)).padStart(3, "0"),
  };
}

// Copy practice: a QSO the way it sounds on the air.
function qsoText(me, dx) {
  me = me || "N0CALL";
  const parts = [
    `CQ CQ CQ DE ${dx.call} ${dx.call} K`,
    `${dx.call} DE ${me} ${me} K`,
    `${me} DE ${dx.call} GM OM TNX FER CALL = UR RST ${dx.rst} ${dx.rst} = NAME ${dx.name} ${dx.name} = QTH ${dx.qth} ${dx.qth} = HW? <AR> ${me} DE ${dx.call} <KN>`,
    `${dx.call} DE ${me} R FB ${dx.name} = RIG HR ${dx.rig} PWR ${dx.pwr}W ANT ${dx.ant} = WX ${dx.wx} TEMP ${dx.temp}C <BK>`,
    `${me} DE ${dx.call} TNX FER QSO = HPE CUL 73 <SK> ${me} DE ${dx.call} TU`,
  ];
  return parts;
}
