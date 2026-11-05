# The Port Atlas feed

`https://www.amicradle.com/ports/ports.json`, written by `render_feed()` in
DalsinAI/amicradle `docs/porting/tools/port_catalogue.py` from
`catalogue.json`. Never edit the published file: add fields in amicradle.

Top level: `schema_version` (1; AmiPortAtlas refuses a newer one and asks
for an update), `generated`, `source`, `ports`.

Each entry (unknown values are `null`; AmiPortAtlas skips keys it doesn't know):

| Field | Used for |
| --- | --- |
| `id` | the key for what is installed (`ENVARC:AmiPortAtlas/Installed`) |
| `number` | the # column; stable, new entries are only appended |
| `origin` | `port`, or `original` for our own games (Canvas Skies) |
| `title`, `year`, `display`, `fit`, `game_data`, `requires[]` | the list and the facts |
| `state` | `not_started`, `in_progress`, `testing`, `released` |
| `version` | the facts; a newer one than installed shows Update |
| `release_url`, `port_repo` | the Release page button |
| `screenshot`, `screenshots[]` | the pictures, cycling; `screenshots` first, main picture first |
| `download_url`, `download_size`, `download_sha256` | the archive; without them the entry is greyed |
| `drawer`, `program` | the game's drawer and the icon that starts it |
| `cpu`, `fpu`, `space`, `modules[]` | the checks before anything is downloaded |

Every other source's index has this same form, with a few more fields
(`kind`, `short`, `author`, `category`, `install`): [SOURCES.md](SOURCES.md).

`runs_on`, `cpu_floor`, `started_on`, `state_on`, `priority` and the rest
are read past for now.
