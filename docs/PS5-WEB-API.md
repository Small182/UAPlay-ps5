# UAPlay PS5: нативний клієнт API UAPlay-web

**Рушій: SDL2 / VideoOut + нативний плеєр PS5. Жодного ProsperoWeb, BrowserPS5 чи залежності від WebKit.**

Це окрема експериментальна гілка від `fw13-main-0510-ui` (Switch v0.5.10).
Вона зберігає наявний PS5 інтерфейс, бібліотеку, DualSense, IPTV, пошук джерел
та нативне відтворення. Вона **не** перетворює HTML/CSS/JS інтерфейс
`UAPlay-web` на PS5 WebView — PS5 WebKit тут навмисно не використовується.

## Доступні API

За наявності адреси Worker native PS5 використовує перевірений у `UAPlay-web/main`
контракт:

| PS5 | Cloudflare Worker |
| --- | --- |
| Головна | `/api/catalog?section=trending&page=N&sort=popularity` |
| Фільми / Серіали | `/api/catalog?section=movies/series&page=N` |
| Мультфільми / Мультсеріали / Аніме | `section=animation/cartoons/anime` |
| Топ / Жанр | `section=top/genre_N` |
| Пошук | `/api/search?q=...&page=N` |
| Інформація про фільм | `/api/title/movie/ID` |
| Інформація про серіал | `/api/title/tv/ID` |

Назви, постери, рейтинги, рік, опис, тривалість і кількість сезонів/серій
перекладаються у наявну нативну структуру `Movie`. Cloudflare
відсіює прем'єри у майбутньому та записи з Росії. Серверні секрети TMDB
залишаються у Worker. Якщо API недоступний, використовується наявний
прямий запит до TMDB та офлайн-кеш, якщо він доступний.

## Налаштування API URL

**Варіант 1: автоматично при складанні в GitHub Actions.** Відкрий
`Small182/UAPlay-ps5 → Settings → Secrets and variables → Actions → Variables`
та додай `UAPLAY_WEB_API_URL` — повний публічний HTTPS origin
розгорнутого UAPlay-web, наприклад
`https://your-deployed-uaplay.example` (тільки приклад, не реальна адреса).
Гілка `fw13-web-api-native` читає цю змінну та компілює її як origin.
GitHub Secret `TMDB_API_KEY` залишається для резервних прямих запитів
у поточному native-build pipeline. Не вказуй токен TMDB в origin.

**Варіант 2: на приставці без повторної збірки.** Поклади в
`/download0/uaplay/web-api-url.txt` один рядок із тією самою адресою.
Файл має пріоритет над адресою, вбудованою під час компіляції.
Зміни застосуються після наступного запиту каталогу.

URL перевіряється: лише `https://` і hostname, без паролів, шляху,
параметрів чи токенів. Якщо адреси немає, код залишається на
початковому TMDB-backend.

## Обмеження поточного кроку

- HTML/CSS/JS із UAPlay-web **не виконуються**: використовується наявний UI PS5.
- Пошук відеоджерел, отримання потоку та нативний плеєр поки залишаються зі
  старої гілки; `/api/providers/search` та `/api/providers/media`
  ще не інтегровані у цей PS5-native шлях.
- IPTV поки з нативного модуля PS5.
- API відповіді перевіряються збіркою/тестами, але робота на реальній PS5
  із прошивкою 13.00 потребує перевірки на пристрої.

## Збірка

`.github/workflows/native-ps5.yml` автоматично компілює
`fw13-web-api-native`, зберігаючи **`PPSA99283.exfat`** та `.ffpkg`
у GitHub Actions artifacts. Встановлюй через звичний ShadowMountPlus.
Це окрема тестова збірка, стабільну гілку вона не замінює.
