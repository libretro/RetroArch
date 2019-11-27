# Internationalization Workflow (Draft)

## Steps

- Developers update strings in `msg_hash_us.h`.
- Developers (can set a cron job) run `./h2json.py msg_hash_us.h` to generate `msg_hash_us.json`. It is just a convenient format that is supported by Weblate/Crowdin/Transifex and doesn't need to be inversion control.
- Developers (can set a cron job) upload `msg_hash_us.json` to Weblate/Crowdin/Transifex.
- Translators translate strings on Weblate/Crowdin/Transifex.
- Developers (can set a cron job) download `msg_hash_xx.json` files.
- Developers (can set a cron job) run `./json2h.py msg_hash_xx.json` to generate `msg_hash_xx.h`.

## Pros

- No new dependencies.
- No performance impact.
- Don't require translators to know how to use Git, how to read C code and how to create Pull Request.
- Translators will be informed whenever a source string changes.
