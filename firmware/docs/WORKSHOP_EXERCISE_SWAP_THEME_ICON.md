# תרגיל סדנה: החלפת אייקון לערכת נושא קיימת

## המטרה

לג'וקבוקס יש 10 ערכות נושא (Themes), וכל אחת מהן מציגה אייקון קטן (16x16 פיקסלים) בשוליים של מסך ה-HOME, בצבע הערכה. במשימה הזו נחליף לערכה אחת את האייקון שלה **באייקון של ערכה אחרת** — לדוגמה, לתת לערכת "Pineapple" (אננס) אייקון של גולגולת במקום אננס — רק בשביל הכיף, ולראות מה קורה.

לא נוגעים בצבעים או בשם הערכה, רק באייקון.

---

## רקע: איפה זה מוגדר

קובץ אחד בלבד מעורב: **`src/JukeboxUI.cpp`**, בפונקציה `drawThemeDecorations()` (בערך שורות 1009–1022):

```cpp
const uint8_t* icon = nullptr;
switch (themeManager.current())
{
    case THEME_PINEAPPLE: icon = iconPineapple; break;
    case THEME_RETRO_ARCADE: icon = iconStar; break;
    case THEME_ROBOT: icon = iconRobot; break;
    case THEME_SPACESHIP: icon = iconSpaceship; break;
    case THEME_PIRATE: icon = iconSkull; break;
    case THEME_VINTAGE_RADIO: icon = iconSpeaker; break;
    case THEME_CASSETTE: icon = iconCassette; break;
    case THEME_FOREST: icon = iconTree; break;
    case THEME_STEAMPUNK: icon = iconGear; break;
    case THEME_MONSTER: icon = iconMonster; break;
    default: break;  // Classic, Custom: no icon
}
```

זה בעצם "טבלת התאמה" פשוטה: כל שורה אומרת "ערכת נושא X מקבלת אייקון Y". **10 האייקונים כבר קיימים** באותו קובץ (בערך שורות 61–70) בתור מערכים בשם `iconPineapple`, `iconStar`, `iconRobot`, `iconSpaceship`, `iconSkull`, `iconSpeaker`, `iconCassette`, `iconTree`, `iconGear`, `iconMonster` — אין צורך ליצור אייקון חדש, רק לבחור אחד קיים.

---

## שלב 1 — בחרו ערכה ואייקון חדש בשבילה

פתחו את `src/JukeboxUI.cpp`, מצאו את הבלוק שלמעלה, ובחרו שורה אחת לשנות. לדוגמה, נחליף את האייקון של Pineapple מאננס לגולגולת:

**לפני:**
```cpp
case THEME_PINEAPPLE: icon = iconPineapple; break;
```

**אחרי:**
```cpp
case THEME_PINEAPPLE: icon = iconSkull; break;
```

(שינוי רק את החלק שאחרי ה-`=` — לא את `case THEME_PINEAPPLE`, ולא את ה-`;` בסוף.)

אפשר כמובן לבחור כל צירוף שרוצים — כל ערכה מותר לה כל אייקון מ-10 הרשימה, גם אייקון ששייך במקור לערכה אחרת.

---

## שלב 2 — קומפילציה, העלאה, ובדיקה

1. שמרו את הקובץ.
2. הריצו `pio run -e esp32s3` ובדקו שאין שגיאות קומפילציה.
3. אם הכול תקין: `pio run -e esp32s3 -t upload` כדי להעלות לבקר.
4. **בדיקה בפועל**: לכו למסך ה-HOME, פתחו את תפריט הערכות (SETTINGS ← THEME) ובחרו את הערכה ששיניתם — האייקון החדש אמור להופיע משני צידי המסך (שוליים שמאל וימין, ליד אזור הכפתורים).

---

## אתגר בונוס (לא חובה)

1. **החלפה כפולה**: תחליפו בין שני אייקונים — למשל תנו ל-Robot את האייקון של Monster, ול-Monster את האייקון של Robot — ותראו את שתי הערכות "מחליפות זהות".
2. **אתגר מתקדם**: מי שרוצה אתגר אמיתי — לנסות לצייר אייקון חדש משלכם! כל אייקון הוא בעצם רשת של 16x16 פיקסלים, כל אחד דלוק (1) או כבוי (0), ארוזה כזוג בייטים לכל שורה (32 בייטים סה"כ לאייקון). זה מסובך יותר ודורש הבנה של מספרים בינאריים/הקסדצימליים — שאלו את המדריך/ה אם אתם רוצים לנסות.
