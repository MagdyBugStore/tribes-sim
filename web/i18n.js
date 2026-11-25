'use strict';
// Arabic localization of the viewer. The simulation sends English transliterations for names,
// items and reasons; ar() turns them into Arabic on the client and leaves unknown text unchanged.
// Numbers stay in Western digits.
(function () {
  const W = {
    // tribes and nisbas
    'Banu Taym': 'بنو تيم', 'Banu Asad': 'بنو أسد', 'Banu Kalb': 'بنو كلب', 'Banu Amir': 'بنو عامر',
    'Banu Sulaym': 'بنو سليم', 'Banu Ghatafan': 'بنو غطفان',
    'al-Taymi': 'التيمي', 'al-Taymiyya': 'التيمية', 'al-Asadi': 'الأسدي', 'al-Asadiyya': 'الأسدية',
    'al-Kalbi': 'الكلبي', 'al-Kalbiyya': 'الكلبية', 'al-Amiri': 'العامري', 'al-Amiriyya': 'العامرية',
    'al-Sulami': 'السلمي', 'al-Sulamiyya': 'السلمية', 'al-Ghatafani': 'الغطفاني', 'al-Ghatafaniyya': 'الغطفانية',
    'Banu': 'بنو',
    // male names
    'Muhammad': 'محمد', 'Ali': 'علي', 'Khalid': 'خالد', 'Zayd': 'زيد', 'Bilal': 'بلال', 'Umar': 'عمر', 'Hamza': 'حمزة',
    'Talha': 'طلحة', 'Zubayr': 'الزبير', "Sa'd": 'سعد', 'Amr': 'عمرو', 'Hafs': 'حفص', 'Jafar': 'جعفر', 'Suhaib': 'صهيب',
    "Nu'man": 'النعمان', 'Waraqa': 'ورقة', 'Usama': 'أسامة', 'Mukhallad': 'مخلد', 'Qays': 'قيس', 'Durayd': 'دريد',
    'Hassan': 'حسان', 'Husayn': 'حسين', 'Abbas': 'عباس', "Mu'awiya": 'معاوية', 'Salman': 'سلمان', 'Ammar': 'عمار',
    'Miqdad': 'المقداد', 'Abdullah': 'عبد الله', 'Ubayda': 'عبيدة', "Mus'ab": 'مصعب', 'Iyas': 'إياس', "Rafi'": 'رافع',
    // female names
    'Fatima': 'فاطمة', 'Aisha': 'عائشة', 'Sumayya': 'سمية', 'Hind': 'هند', 'Layla': 'ليلى', 'Salma': 'سلمى',
    'Zaynab': 'زينب', 'Ruqayya': 'رقية', 'Khadija': 'خديجة', 'Maymuna': 'ميمونة', "Rabi'a": 'ربيعة', 'Baraka': 'بركة',
    'Halima': 'حليمة', 'Safiyya': 'صفية', 'Hafsa': 'حفصة', 'Umm Kulthum': 'أم كلثوم', 'Juwayriyya': 'جويرية',
    'Sawda': 'سودة', 'Asma': 'أسماء', 'Nusayba': 'نسيبة', 'Umm Salama': 'أم سلمة', 'Ghazala': 'غزالة', 'Rayhana': 'ريحانة',
    'Lubna': 'لبنى', 'Sakina': 'سكينة', 'Durra': 'درة', 'Amat al-Aziz': 'أمة العزيز', 'Ward': 'ورد', 'Narjis': 'نرجس',
    'Mariam': 'مريم', 'Dunya': 'دنيا',
    // roles
    GrandSheikh: 'شيخ المشايخ', Sheikh: 'شيخ', Imam: 'إمام', Muezzin: 'مؤذن', Qadi: 'قاضٍ', Muallim: 'معلم', Hakim: 'حكيم',
    Qabila: 'قابلة', Shair: 'شاعر', Haris: 'حارس', Faris: 'فارس', Khazin: 'خازن', Overseer: 'مشرف', Nakhali: 'نخّال',
    WellDigger: 'حفّار آبار', Tajir: 'تاجر', Ajer: 'أجير', Fallah: 'فلّاح', WellKeeper: 'حارس البئر', Herder: 'راعٍ',
    CamelHerder: 'راعي إبل', Dairyman: 'لبّان', Hunter: 'صيّاد', Falconer: 'صقّار', Miller: 'طحّان', Baker: 'خبّاز',
    Weaver: 'نسّاج', Tanner: 'دبّاغ', Haddad: 'حدّاد', SaltDigger: 'مستخرج الملح', Hattab: 'حطّاب', Jazzar: 'جزّار',
    Khaddam: 'خادم', Caravaneer: 'قائد قافلة', CamelDriver: 'حادي إبل', Hakawati: 'حكواتي', Muzayyin: 'مزيّن', Dalil: 'دليل',
    Tilmidh: 'تلميذ', Child: 'طفل', Elder: 'شيخ كبير',
    // items, livestock
    water: 'ماء', bread: 'خبز', wheat: 'قمح', flour: 'دقيق', bran: 'نخالة', dates: 'تمر', milk: 'حليب', laban: 'لبن',
    cheese: 'جبن', meat: 'لحم', salt: 'ملح', hide: 'جلد خام', leather: 'جلد', sandals: 'نعال', waterskin: 'قربة', wool: 'صوف',
    yarn: 'غزل', rug: 'سجاد', tent: 'خيمة', iron: 'حديد', tools: 'أدوات', blade: 'نصل', pottery: 'فخار', cloth: 'قماش',
    silk: 'حرير', incense: 'بخور', spice: 'توابل', sugar: 'سكر', coffee: 'قهوة', oil: 'زيت', firewood: 'حطب',
    camel: 'جمل', horse: 'فرس', cattle: 'بقر', goat: 'ماعز', sheep: 'غنم', donkey: 'حمار', chicken: 'دجاج',
    well: 'بئر', palm: 'نخلة', field: 'حقل', saltpan: 'ملّاحة',
    // units
    liter: 'لتر', piece: 'قطعة', kg: 'كجم', meter: 'متر', bolt: 'طاقة', load: 'حِمل', head: 'رأس', hectare: 'هكتار',
    // stages, genders, seasons
    adult: 'بالغ', child: 'طفل', elder: 'مسن', youth: 'شاب', infant: 'رضيع', male: 'ذكر', female: 'أنثى',
    winter: 'شتاء', spring: 'ربيع', summer: 'صيف', autumn: 'خريف',
    // origins
    Yemen: 'اليمن', Makkah: 'مكة', 'al-Sham': 'الشام',
    // map places
    mosque: 'المسجد', majlis: 'المجلس', suq: 'السوق', caravanserai: 'خان القوافل', school: 'الكُتّاب', clinic: 'المستوصف',
    mill: 'الطاحونة', oven: 'الفرن', smithy: 'الحدادة',
    // causes
    illness: 'المرض', 'childhood illness': 'مرض الطفولة', 'old age': 'الشيخوخة',
  };
  const keys = Object.keys(W).sort((a, b) => b.length - a.length);
  const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const NAMES = new RegExp('(^|[^A-Za-z\'\\-])(' + keys.filter((k) => /[A-Z]|[a-z]/.test(k)).map(esc).join('|') + ")(?![A-Za-z'])", 'g');
  function words(s) { return s.replace(NAMES, (m, pre, w) => pre + W[w]); }

  // phrase patterns: [regex, builder(match) -> Arabic with raw pieces still English]
  const P = [
    [/^thirsty \(thirst (\d+) of 100\): going to the well$/, (m) => `عطشان (العطش ${m[1]} من 100): ذاهب إلى البئر`],
    [/^hungry \(hunger (\d+) of 100\): (.*)$/, (m) => `جائع (الجوع ${m[1]} من 100): ${m[2]}`],
    [/^hungry \(([^)]*)\): going to eat$/, (m) => `جائع (${m[1]}): ذاهب ليأكل`],
    [/^sick: resting at home$/, () => 'مريض: يرتاح في البيت'],
    [/^night: sleeping$/, () => 'الليل: نوم'],
    [/^prayer time$/, () => 'وقت الصلاة'],
    [/^mealtime$/, () => 'وقت الطعام'],
    [/^school hours(.*)$/, (m) => 'ساعات الكُتّاب' + m[1]],
    [/^caring for the family$/, () => 'رعاية الأسرة'],
    [/^free time with others$/, () => 'وقت فراغ مع الناس'],
    [/^work of a (\S+) during the working day$/, (m) => `عمل ${words(m[1])} خلال النهار`],
    [/^midday rest: the heat is too strong.*$/, () => 'استراحة الظهيرة: الحرّ شديد'],
    [/^retired: rests.*$/, () => 'متقاعد: يرتاح'],
    [/^free time at home$/, () => 'وقت فراغ في البيت'],
    [/^Friday prayer.*$/, () => 'صلاة الجمعة'],
    [/^meeting a need$/, () => 'قضاء حاجة'],
    [/^Day (\d+): (.*) dies of illness at (\d+)$/, (m) => `اليوم ${m[1]}: توفي ${words(m[2])} بسبب المرض وعمره ${m[3]}`],
    [/^Day (\d+): (.*) dies of childhood illness at (\d+)$/, (m) => `اليوم ${m[1]}: توفي ${words(m[2])} بمرض الطفولة وعمره ${m[3]}`],
    [/^Day (\d+): (.*) dies of old age at (\d+)$/, (m) => `اليوم ${m[1]}: توفي ${words(m[2])} من الشيخوخة وعمره ${m[3]}`],
    [/^Day (\d+): (.*) marries (.*) \(mahr (\d+)\)$/, (m) => `اليوم ${m[1]}: تزوج ${words(m[2])} من ${words(m[3])} (المهر ${m[4]})`],
    [/^Day (\d+): (?:a|the) caravan from (.*) arrives, a small one$/, (m) => `اليوم ${m[1]}: وصلت قافلة صغيرة من ${words(m[2])}`],
    [/^Day (\d+): (?:a|the) caravan from (.*) arrives ruined.*$/, (m) => `اليوم ${m[1]}: وصلت قافلة من ${words(m[2])} خاسرة، فقد أتلفت الرمال حمولتها`],
    [/^Day (\d+): (?:a|the) caravan from (.*) arrives$/, (m) => `اليوم ${m[1]}: وصلت قافلة من ${words(m[2])}`],
    [/^Day (\d+): the Majlis could not pay its people in full$/, (m) => `اليوم ${m[1]}: عجز المجلس عن دفع أجور أهله كاملة`],
    [/^Day (\d+): the Majlis gives the Eid feast$/, (m) => `اليوم ${m[1]}: المجلس يقيم وليمة العيد`],
    [/^Day (\d+): the zakat is taken \((\d+) dirhams\)$/, (m) => `اليوم ${m[1]}: تُجبى الزكاة (${m[2]} درهم)`],
    [/^Day (\d+): (.*) gave birth to (.*)$/, (m) => `اليوم ${m[1]}: ولدت ${words(m[2])} ${words(m[3])}`],
    [/^Day (\d+): gave birth to (.*)$/, (m) => `اليوم ${m[1]}: ولدت ${words(m[2])}`],
    [/^Day (\d+): looks in on an old neighbor$/, (m) => `اليوم ${m[1]}: يتفقد جارًا مسنًّا`],
    [/^Day (\d+): received (\S+) from person (\d+) \(inheritance \(fara'id\)\)$/, (m) => `اليوم ${m[1]}: استلم ${m[2]} من الشخص ${m[3]} (ميراث، الفرائض)`],
    [/^Day (\d+): received (\S+) from person (\d+) \(mahr\)$/, (m) => `اليوم ${m[1]}: استلم ${m[2]} من الشخص ${m[3]} (مهر)`],
    [/^Day (\d+): paid (\S+) to person (\d+) \(mahr\)$/, (m) => `اليوم ${m[1]}: دفع ${m[2]} إلى الشخص ${m[3]} (مهر)`],
    [/^Day (\d+): received (\S+) ([A-Za-z]+) \(inheritance\)$/, (m) => `اليوم ${m[1]}: استلم ${m[2]} ${words(m[3])} (ميراث)`],
  ];
  const UI = {
    'Day': 'اليوم', 'Season': 'الموسم', 'People': 'السكان', 'Pause': 'إيقاف مؤقت', 'Tribe': 'القبيلة',
    'All tribes': 'كل القبائل', 'Role group': 'فئة المهنة', 'All roles': 'كل المهن', 'Activity': 'النشاط',
    'All activities': 'كل الأنشطة', 'Labels': 'الأسماء', 'Animals': 'الحيوانات', 'connecting...': 'جارٍ الاتصال...',
    'live': 'مباشر', 'waiting for the simulation...': 'بانتظار المحاكاة...', 'Person unavailable': 'الشخص غير متاح',
    'Find by name or id': 'ابحث بالاسم أو الرقم', 'Dashboards': 'لوحات المتابعة',
    'Economy, market, herds, caravans, alerts': 'الاقتصاد والسوق والقطعان والقوافل والتنبيهات',
    'Zoom in': 'تكبير', 'Zoom out': 'تصغير', 'Fit the valley': 'عرض الوادي كله', 'Close': 'إغلاق',
    'Economy': 'الاقتصاد', 'Market': 'السوق', 'Herds': 'القطعان', 'Caravans': 'القوافل', 'Alerts': 'التنبيهات',
    'Children': 'الأطفال', 'Farming and water': 'الزراعة والماء', 'Herding and dairy': 'الرعي والألبان',
    'Crafts and labor': 'الحرف والعمل', 'Trade and caravans': 'التجارة والقوافل', 'Council and arts': 'المجلس والفنون',
    'Mosque and school': 'المسجد والكُتّاب', 'Guards and hunters': 'الحرس والصيادون', 'Healers': 'المداوون', 'Elders': 'كبار السن',
    'People ': 'السكان', 'Valley': 'الوادي', 'Ring: women and girls': 'حلقة: النساء والبنات',
    'Small dot: children': 'نقطة صغيرة: الأطفال', 'Tiny dots: herd animals': 'نقاط دقيقة: حيوانات القطيع',
    'Well': 'بئر', 'Field': 'حقل', 'Palm grove': 'نخيل', 'Pasture': 'مرعى', 'Suq': 'سوق', 'Mosque': 'مسجد',
    'Majlis': 'مجلس', 'Caravan road': 'طريق القوافل', 'sick': 'مرضى', 'expecting': 'حامل', 'retired': 'متقاعد',
    'deceased': 'متوفى', 'Following': 'يتابَع', 'Follow': 'تابِع', 'Right now': 'الآن', 'Status': 'الحالة', 'Needs': 'الحاجات',
    'hunger': 'الجوع', 'thirst': 'العطش', 'energy': 'الطاقة', 'health': 'الصحة', 'happiness': 'السعادة',
    'Honor': 'الشرف', 'Skill': 'المهارة', 'Money and goods': 'المال والمتاع', 'Pouch': 'الكيس', 'dirhams': 'درهم',
    'Wadiah (savings)': 'الوديعة (مدخرات)', 'Qard (debt)': 'القرض (دين)', 'Today': 'اليوم', 'Family': 'العائلة',
    'Spouse': 'الزوج', 'Father': 'الأب', 'Mother': 'الأم', 'Siblings': 'الإخوة', 'none': 'لا أحد', 'History': 'السجل',
    'nothing yet': 'لا شيء بعد', 'at': 'في', '(animals)': '(حيوانات)',
    'Item': 'السلعة', 'Price': 'السعر', 'vs base': 'مقابل الأساس', '7 d': '7 أيام', 'Shop stock': 'مخزون الدكاكين',
    'Days of stock': 'أيام المخزون', 'Animal': 'الحيوان', 'Total': 'المجموع', 'Born 30 d': 'مواليد 30 يومًا',
    'Slaughtered 30 d': 'مذبوح 30 يومًا', 'Arrived': 'الوصول', 'From': 'من', 'Stay': 'المكث', 'Cargo units': 'وحدات الحمولة',
    'Largest cargo': 'أكبر حمولة', 'Sold to locals': 'بيع للأهالي', 'Bought from locals': 'شراء من الأهالي', 'Result': 'النتيجة',
    'failed: cargo lost': 'فشلت: ضاعت الحمولة', 'ok': 'سليمة', 'waiting for the first day...': 'بانتظار اليوم الأول...',
    'no caravan has arrived yet': 'لم تصل قافلة بعد', 'event': 'حدث', 'birth': 'ولادة', 'death': 'وفاة', 'money': 'مال',
    'caravan': 'قافلة', 'jumuah': 'جمعة', 'famine': 'مجاعة', 'wedding': 'زواج', 'well': 'بئر', 'eid': 'عيد', 'alert': 'تنبيه', 'total (constant)': 'المجموع (ثابت)', 'pouches': 'الأكياس', 'Bayt al-Mal': 'بيت المال',
    'shops': 'الدكاكين', 'caravans': 'القوافل', 'Majlis + mint': 'المجلس + دار السك', 'gini': 'جيني', 'hungry': 'جائعون',
    'thirsty': 'عطشى', 'births': 'مواليد', 'deaths': 'وفيات',
    'Money supply and where it sits (dirhams)': 'المعروض النقدي وأين يستقر (بالدرهم)', 'Gini of wealth': 'معامل جيني للثروة',
    'Hungry, thirsty and sick people': 'الجائعون والعطشى والمرضى', 'Births and deaths per week': 'المواليد والوفيات أسبوعيًا',
    'Click an item for its price history (last 90 days)': 'اضغط على سلعة لعرض تاريخ سعرها (آخر 90 يومًا)',
    'Born': 'مواليد',
  };
  function num(s) { return s; }
  function ar(s) {
    if (s === null || s === undefined) return '';
    s = String(s);
    if (UI[s] !== undefined) return UI[s];
    for (const [re, f] of P) { const m = re.exec(s); if (m) return f(m); }
    if (!/^Day \d+: /.test(s)) {
      const t = ar('Day 0: ' + s);
      if (t.startsWith('اليوم 0: ')) return t.slice(9);
    }
    return words(s);
  }
  // Arabic text for a sentence with a leading number etc is handled by callers through ar() pieces.
  window.AR = { ar, words, W, UI };
  window.ar = ar;
})();
