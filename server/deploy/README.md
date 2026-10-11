# 전화 교환기 — 맥미니 운영 (Phase 4 + 신호 규약 v2)

전화기들(Tab5 1번·2번 + 스택짱 2호기 — 등록 2~8대)이 인터넷을 건너 서로 골라 걸게 하는 **교환기(WebSocket 중계)** 를 맥미니에 세우고 돌보는 방법이다.
기기 쪽(`wss://` 연결·`RELAYERR` 줄)은 `firmware/tab5_phone/README.md` 의 "교환기 연결" 절. 결정 ID 는 `docs/exec-plans/2026-10-05-tab5-phone-phase4-relay.md` 와 `phone_relay_prod.sh` 머리 주석(P4-D1~D3·P4N-D8~D12), 신호 규약 v2 는 `docs/exec-plans/2026-10-08-phone-3way-design.md` §6·§7(TW-D7~D10).

## 구성

```
기기 ──wss://phone.example.com/ws──> Cloudflare ──전용 터널 tab5phone──> 맥미니 127.0.0.1:5094 (phone_relay.py)
```

| 무엇 | 이름 | 비고 |
|---|---|---|
| 교환기 launchd | `com.example.tab5phone-relay` | `127.0.0.1:5094` 에만 연다(plist 환경으로 `BIND`·`PORT` 고정 — `.env.phone` 에 개발용 `BIND=0.0.0.0` 이 섞여도 밖으로 열리지 않는다). plist 에 토큰은 없다(교환기가 `server/.env.phone` 을 읽는다) |
| 터널 launchd | `com.example.tab5phone-tunnel` | **전용 터널 `tab5phone`** → 주소 `phone.example.com`. 설정 `~/.cloudflared/tab5phone.yml`, credentials `~/.cloudflared/tab5phone.credentials.json` |
| 로그 | `~/Library/Logs/tab5phone/relay.log`·`tunnel.log` | 회전 없음 |
| 공용 터널 `maintunnel` | (건드리지 않는다) | 다른 서비스 20여 개가 쓴다. 전용 터널로 가른 까닭(P4-D2): 거기에 호스트를 더하면 cloudflared 재시작 때 다른 서비스가 몇 초 끊기고, 앞으로 그 터널이 재시작될 때마다 통화가 끊긴다. 비용: 프로세스·plist 하나 더. 탈출구: `uninstall` 후 터널을 지우고 기존 설정에 호스트 한 덩이를 넣는다 |

터널 UUID 는 `00000000-0000-0000-0000-000000000000`(2026-10-05 15:07 만듦, 비밀이 아니다).

## 신호 규약 v2 — 기기 2~8대, 거는 상대 지정 (2026-10-08)

정본은 설계서 `docs/exec-plans/2026-10-08-phone-3way-design.md` §6 이고, 교환기 시험(`server/tests/test_phone_core_v2.py`·`test_phone_relay_v2.py`)이 고정한다. 운영에서 알아야 할 것만 적는다.

| 무엇 | 내용 |
|---|---|
| 규약 구분(TW-D7) | 기기가 접속 헤더 `X-Phone-Proto: 2` 를 보내면 v2 연결, 없거나 그 밖이면 v1 연결(옛 펌웨어). 교환기는 둘을 함께 받는다 |
| v2 연결 | 거는 상대를 고른다(`call` 의 `to`), 벨에 건 기기가 실린다(`ring` 의 `from`), 접속 직후 다른 기기 **전부**의 켜짐/꺼짐을 기기 ID 와 함께 받는다 |
| v1 연결(TW-D8) | **옛 짝**하고만 걸고 받는다 — 프레임은 글자까지 예전과 같다. 옛 짝이 아닌 기기에게 v1 기기는 "꺼짐"으로 보이고, 걸면 `ended: offline` |
| 통화 중(TW-D9) | 벨이 울리는 중이거나 통화 중인 상대에게 걸면 거는 쪽만 `ended: busy` 를 받는다 |
| 통화 여러 건(TW-D10) | 통화는 기기 쌍마다 따로다. 한 통화의 끊김·이탈·벨 제한(30초)이 다른 통화에 닿지 않는다. 한 기기는 많아야 한 통화 |
| 그대로인 것 | `127.0.0.1` 에만 연다 · `/healthz` 는 `{"ok": true}` 만 · 닫기 코드(1001) · 인증(401)·인증 실패 기록 창 · 45초 무소식 끊기 · 기록에는 시각·기기·통화 길이·사유만 |

### 설정 `server/.env.phone` (git 제외, 권한 600)

| 줄 | 뜻 |
|---|---|
| `PHONE_TOKENS=tab5-1:<토큰>,tab5-2:<토큰>,stackchan-2:<토큰>` | 등록 기기 **2~8대**, 토큰 16자 이상. **적힌 순서가 뜻을 가진다** — 옛 짝의 기본값이 앞 두 대다. 새 기기는 끝에 붙인다(도구가 그렇게 한다) |
| `PHONE_LEGACY_PAIR=tab5-1,tab5-2` | (선택) **옛 짝** = v1 연결끼리의 상대, 그리고 `to` 없는 `call` 이 가는 곳. 없으면 `PHONE_TOKENS` 에 적힌 순서의 앞 두 대. 등록되지 않은 ID·같은 ID 둘·개수가 틀리면 교환기가 시작하지 않는다 |
| `PORT`·`BIND` | 운영에서는 plist 환경(127.0.0.1:5094)이 이긴다 |

값이 틀리면 교환기는 `[phone_relay] 설정 오류: …` 한 줄로 끝난다(토큰은 찍지 않는다). launchd 가 10초마다 다시 띄우려 하므로 `relay.log` 에 그 줄이 되풀이된다 — **`.env.phone` 을 고친 뒤 재시작하기 전에 아래 "예행연습"으로 먼저 읽혀 보는 까닭**이다.
정상으로 뜨면 `relay.log` 첫 줄이 `listening http://127.0.0.1:5094 devices=tab5-1,tab5-2,stackchan-2 legacy_pair=tab5-1,tab5-2` 꼴이다.

### 교환기를 v2 로 올리는 순서

**순서가 중요하다: 코드 먼저, 토큰 나중.** 옛 코드는 토큰이 정확히 2개가 아니면 시작하지 않는다 — 옛 코드가 도는 동안 `.env.phone` 에 셋째 토큰을 넣으면, 다음 재시작(재부팅·크래시 포함)에 교환기가 뜨지 못한다. 새 코드는 2대 설정도 그대로 받는다.

1. **예행연습(운영 교환기를 건드리지 않는다)** — 새 코드를 빈 포트에 따로 띄워 가짜 기기로 돌려 본다. 진짜 기기는 5094 에 붙어 있으므로 쫓겨나지 않는다.
   ```
   lsof -nP -iTCP:5095 -sTCP:LISTEN            # 비어 있어야 한다
   cd ~/tab5-toy-phone/server && PORT=5095 BIND=127.0.0.1 python3 phone_relay.py      # 다른 터미널에서. 설정이 틀리면 여기서 "설정 오류" 한 줄로 끝난다
   python3 tools/phone_smoke.py http://127.0.0.1:5095            # 2대(옛 짝, v1) → SMOKE OK
   python3 tools/phone_smoke.py http://127.0.0.1:5095 --three    # 3대 등록일 때만 → THREE OK (고른 상대에게만 벨 · busy · v1/v2 섞임)
   ```
   끝나면 5095 의 교환기를 Ctrl-C 로 끈다.
2. **통화 중이 아닐 때** 코드를 반영하고 `server/deploy/phone_relay_prod.sh restart relay`(터널은 건드리지 않는다). 붙어 있던 기기는 1~2초 끊겼다 3~18초 안에 다시 붙고, 통화 중이었다면 그 통화는 끝난다.
   - 기록으로는 "지금 통화 중"을 알 수 없다: `relay.log` 에는 **끝난** 통화(`call_end`)만 남는다. 기기 화면이나 기기의 `STAT` 으로 확인한다.
3. 확인: `phone_relay_prod.sh status`(둘 다 200) → `relay.log` 의 `listening … devices=… legacy_pair=…` 줄 → 진짜 기기의 `{"ev":"online","dev":"tab5-2"}` 줄. 옛 펌웨어의 Tab5 끼리는 예전과 똑같이 통화된다(v1).
4. 그다음에 아래 "기기 한 대 더하기".

**되돌리기**: 옛 코드로 돌아갈 때는 **먼저** `.env.phone` 의 `PHONE_TOKENS` 에서 셋째 기기를 빼고(`PHONE_LEGACY_PAIR` 줄은 옛 코드가 읽지 않으니 두어도 된다) 코드를 되돌린 뒤 `restart relay`. 순서를 바꾸면 옛 코드가 "정확히 2대"에 걸려 뜨지 못한다.

### 기기 한 대 더하기 (기존 토큰은 그대로)

전제: 교환기 코드가 v2 다(위 순서). 예: 스택짱 2호기(기기 ID `stackchan-2`, 번호 3).

1. **(사용자가 직접 — 비밀 값을 쓰는 일이라 Claude Code 의 자동 안전 검사가 막는다)** 두 파일(`server/.env.phone`·`firmware/tab5_phone/main/secrets.h`)이 함께 있는 기기에서:
   ```
   cd ~/tab5-toy-phone && python3 server/tools/phone_token_rotate.py --add-device stackchan-2 --unit 3 --mac AA:BB:CC:DD:EE:03
   python3 server/tools/phone_token_rotate.py --check
   ```
   기대: `기기 더함: stackchan-2(번호 3) — 새 토큰 1개(43자), …` · `기존 기기 2대의 토큰: 그대로` · `보관: …`, 이어 `--check` 가 `tab5-1: 일치`·`tab5-2: 일치`·`stackchan-2: 일치`.
   - 도구는 새 토큰 하나를 만들어 `PHONE_TOKENS` 값 **끝에** `,stackchan-2:<토큰>` 을 붙이고, `secrets.h` 의 `PHONE_DEVICES` 표 마지막 줄 뒤에 `{ "<MAC>", 3, "stackchan-2", "<토큰>" }, \` 한 줄을 넣는다. **기존 토큰은 한 글자도 바꾸지 않는다** — 집에 있어 다시 올릴 수 없는 1번 Tab5 가 끊기지 않게. 쓰기 전에 새 글을 교환기의 해석기로 다시 읽어 "기존 기기의 ID·토큰·순서가 그대로"임을 확인하고, 어긋나면 아무것도 쓰지 않는다.
   - 이미 있는 기기면 아무것도 바꾸지 않고 `이미 등록된 기기` 라고 말한다(다시 실행해도 안전하다). 한쪽 파일에만 있으면 스스로 맞추지 않고 멈춘다.
   - 출력에 토큰·MAC·주소는 없다. 바꾸기 전 두 파일은 `firmware/tab5_phone/out/secrets_backup/<시각>/` 에 보관된다 — **되돌리기**: 그 두 파일을 원래 자리에 복사.
   - **인자 없이 실행하면 "교체"다 — 등록된 모든 기기의 토큰이 바뀐다.** 다시 올릴 수 없는 기기가 있는 동안에는 쓰지 않는다.
2. 두 파일은 git 에 오르지 않는다 → **다른 기기에는 파일째로 옮긴다**(`scp`). 교환기가 도는 맥미니의 `.env.phone`, 펌웨어를 빌드하는 기기(맥미니·맥북 둘 다)의 `secrets.h` 가 같은 값이어야 한다. 옮긴 기기에서 `--check` 로 확인한다.
3. 위 "예행연습"을 한 번 더 한다(이번에는 `--three` 까지) → **통화 중이 아닐 때** `phone_relay_prod.sh restart relay`. 토큰은 교환기가 뜰 때 한 번 읽는다 — 재시작 전에는 새 기기가 401("기기 등록 오류")로 거절된다.
4. 확인: `relay.log` 의 `listening … devices=tab5-1,tab5-2,stackchan-2 legacy_pair=tab5-1,tab5-2`, 새 기기가 붙으면 `{"ev":"online","dev":"stackchan-2"}`.

진짜 기기 한 대와 가짜 기기로 화면을 볼 때(펌웨어 README): `python3 server/tools/phone_fake_peer.py --proto 2 stackchan-2 expect:peer:tab5-2:on send:call:tab5-2 expect:ringing …`. `--proto 2` 를 빼면 옛 펌웨어 흉내(v1)다. **`send:call` 은 그 교환기에 붙어 있는 진짜 기기의 벨을 울린다** — 사람이 있는 곳의 기기(집의 1번)를 부르지 않게 대상을 확인한다.

## 스크립트 `phone_relay_prod.sh` (맥미니에서 실행)

```
server/deploy/phone_relay_prod.sh <낱말> [relay|tunnel] [--dry-run]
```

| 낱말 | 하는 일 |
|---|---|
| `install` | 교환기 launchd 항목 만들기·등록·시작. `server/.env.phone` 이 없거나 `PHONE_TOKENS` 줄이 없거나 5094 를 다른 프로세스가 쓰고 있으면 멈춘다. `.env.phone` 권한을 600 으로 조인다 |
| `tunnel-install` | 전용 터널 만들기(없으면) → 설정 → 검사 → DNS → launchd 항목 등록·시작. 전제: `~/.cloudflared/cert.pem` |
| `start` · `stop` · `restart` · `status` | 교환기·터널 각각. 뒤에 `relay` 또는 `tunnel` 을 쓰면 그쪽만 |
| `uninstall` | 둘 다 내리고 plist 삭제. **터널·DNS 는 지우지 않고** 안내만 출력 |
| `--dry-run` | 실행할 명령만 출력(읽기 전용 확인은 한다) |

여러 번 실행해도 같은 결과다(plist 가 같고 이미 떠 있으면 "이미"라고 말하고 그대로 둔다 — 재실행이 통화를 끊지 않게). `restart` 는 `bootout` → `bootstrap` 이다(SIGKILL 이 아니라서 교환기가 정상 종료하며 붙은 기기에 닫기 코드 1001 을 보낸다). `stop` 한 뒤 재부팅하면 다시 뜬다.

**교환기만 고칠 때는 `restart relay`** — 터널을 다시 시작하면 통화가 끊긴다.

### 맥미니 cloudflared 이름 함정 (실제로 만났다)

이 맥미니에서는 `cloudflared tunnel info <이름>` 이 기본 설정의 credentials 를 먼저 읽어 **이름과 상관없이 공용 터널 `maintunnel` 의 ID 를 돌려준다**(`route dns`·`delete`·`run` 도 같은 길). `--dry-run` 에서 "터널 이미 있음: tab5phone" 으로 나타났고(실제로는 없었다), 그대로 설치했다면 공용 터널에 전화기 전용 ingress 를 가진 둘째 연결이 붙어 다른 서비스 요청 일부가 404 가 될 뻔했다. 그래서 스크립트는:
- 존재 확인은 `tunnel list --name tab5phone --output json` 으로 하고(없으면 `null`), 이후 `info`·`route dns`·`delete`·`run` 은 **UUID 로만** 부른다. 얻은 UUID 가 공용 설정의 터널 값과 같으면 중단한다.
- `tunnel create` 에는 전용 credentials 경로를 명시하고, 앞뒤로 공용 `config.yml`·공용 credentials 의 해시를 비교해 달라지면 중단한다.
- **손으로 cloudflared 를 부를 때도 이름으로 부르지 말고 UUID 로만 부른다.** 지울 때(`uninstall` 뒤 안내)도 이름으로 지우면 공용 터널이 지워질 수 있다.

## 처음 올리는 순서

1. **(사용자가 직접, 맥북)** 토큰 교체 — *처음 세울 때만 하는 단계다. 교체는 등록된 모든 기기의 토큰을 바꾸므로, 기기가 이미 돌고 있을 때 한 대를 더하려면 이 단계가 아니라 위 "기기 한 대 더하기"를 쓴다*: `cd ~/tab5-toy-phone && python3 server/tools/phone_token_rotate.py --relay-url wss://phone.example.com/ws && python3 server/tools/phone_token_rotate.py --check` → 기대 `tab5-1: 일치`·`tab5-2: 일치`·`RELAY_URL: wss, phone.example.com`. **Claude Code 가 대신 실행하지 못한다** — 자동 안전 검사가 비밀 값 쓰기를 막는다(2026-10-05 막혔다). 도구는 새 토큰 둘을 `server/.env.phone` 과 `firmware/tab5_phone/main/secrets.h` 에 함께 쓰고, 먼저 검증하고(기기 줄이 정확히 1개씩 아니면 아무것도 안 씀), 옛 파일을 `firmware/tab5_phone/out/secrets_backup/<시각>/`(권한 0600)에 보관하고, 임시 파일 + `os.replace` 로 쓴다. 출력은 길이·줄 수·바꿈/그대로뿐이고 값은 안 찍는다. **되돌리기**: 보관 폴더의 두 파일을 `server/.env.phone`·`secrets.h` 로 복사(사용자가 직접). **교체하면 파일의 토큰만 새것이 된다** — 맥북 개발 교환기는 메모리에 옛 토큰을 들고 있어 **재시작하지 않는 한** 지금 기기(옛 토큰)와 계속 통화된다. **두 대 다 wss 로 붙기 전에는 맥북 교환기를 재시작하지 않는다**(한 대 먼저 올린 창에 재시작하면 아직 안 구운 기기가 "기기 등록 오류"가 된다). 조합 표는 펌웨어 README "교환기 연결" 절.
2. **(사용자가 직접)** `.env.phone` 은 gitignore 라 devsync 가 안 옮긴다: `scp ~/tab5-toy-phone/server/.env.phone macmini:~/tab5-toy-phone/server/.env.phone`.
3. 맥미니: `cd ~/tab5-toy-phone && git pull --ff-only` → `server/deploy/phone_relay_prod.sh install` → 끝줄 `교환기 응답 확인: http://127.0.0.1:5094/healthz 200`. 실패하면 `relay.log`.
4. `server/deploy/phone_relay_prod.sh tunnel-install` — 이미 설치된 맥미니(2026-10-05 이후)에서는 "이미"로 끝나도록 만들었다(다시 돌려 본 실측은 없다 — 처음 설치만 아래 "지금 상태"에 기록).
5. `server/deploy/phone_relay_prod.sh status` → 두 서비스 `실행 중 (pid …)`, 로컬·https healthz 200.
   - **설치 뒤 확인 순서(wss 로 기기를 올리기 전)**: status → `phone_smoke --stream 30`(아래 6) → 기기를 wss 로 올린 뒤 **교환기 재시작(`restart relay`) 10회** 를 하며 `RELAY up=1` 로 돌아오는지·`heap_int_min` 이 내려가지 않는지 보고, 마지막에 통화 한 번 뒤 `heap_int_min ≥ 100,000`(펌웨어 README "교환기 연결" 절 판정선).
6. **터널 통화 확인**(맥북, 기기가 그 주소에 붙어 있지 않을 때): `python3 server/tools/phone_smoke.py wss://phone.example.com/ws --stream 30` → 끝에 `STREAM OK`. 가짜 기기 둘(옛 짝, v1)이 걸고 받고 20ms 간격 40~130바이트 묶음을 양방향 동시에 흘려 방향별 보냄/받음/손실/순서 뒤바뀜/중복, 편도 지연(중앙·90%·최대), ping 왕복을 출력한다. 멈춤 기준: 손실 5% 초과 또는 편도 중앙값 300ms 초과면 보고. **진짜 기기 ID(`tab5-1`·`tab5-2`, `--three` 는 `stackchan-2` 까지)로 붙어 진짜 기기를 쫓아낸다**(같은 ID 로 붙으면 옛 연결이 끊긴다) — 기기가 그 교환기에 붙어 있지 않을 때만. 등록 기기가 3대여도 이 시나리오는 옛 짝 두 대로 그대로 돈다.

## 지금 상태 (2026-10-05)

- **신호 규약 v2(2026-10-08)**: 코드·시험·도구는 준비됐고 **운영 반영 전**이다 — 맥미니에 떠 있는 교환기는 아직 2대 고정의 옛 코드다. 반영하면 이 줄을 날짜·결과(예행연습의 `THREE OK`, `relay.log` 의 `listening … devices=… legacy_pair=…`, 진짜 기기의 재접속)로 바꾼다. 가짜 기기로만 확인했고 진짜 기기(옛 펌웨어의 Tab5·새 펌웨어·스택짱)와는 맞춰 보지 않았다.

- **전용 터널은 맥미니에 설치돼 떠 있다**(15:07): 터널 `tab5phone` 생성(UUID 위), `phone.example.com` DNS 등록, launchd `com.example.tab5phone-tunnel` running(연결 4). **공용 터널 불변을 확인했다**: `config.yml`·공용 credentials 의 수정 시각·크기가 실행 전후 같고, 공용 터널 프로세스(`com.example.cloudflared`)가 그대로이며, 다른 호스트 세 곳이 전후 200 이었다. 공용 설정은 실행 전 `config.yml.bak.tab5phone.<시각>` 로 보관했다.
- 그때 `https://phone.example.com/healthz` 는 502 였다(교환기가 아직 없어서 정상).
- **교환기(`com.example.tab5phone-relay`)가 맥미니에 떠 있다고 쓰지 않는다 — 설치 전이다**(맥미니에 `server/.env.phone` 이 없다: 위 1·2 단계가 사용자 손을 기다린다).
  - **설치 결과(2026-10-05 17:12)**: 사용자가 토큰 교체·`scp` 를 실행한 뒤 `install` → `교환기 응답 확인: http://127.0.0.1:5094/healthz 200`, `status` = 교환기·터널 실행 중, `https://phone.example.com/healthz` 200, 포트는 `127.0.0.1:5094` 에만. `phone_smoke.py wss://phone.example.com/ws --stream 30` = `STREAM OK`(양방향 1500/1500, 손실 0, 편도 중앙값 86.9·125.9ms, ping 왕복 중앙값 128.4ms — 두 가짜 기기 모두 맥북). 기기 두 대가 붙은 뒤: 교환기 `restart relay` 5회 — 기기는 종료 약 0.4초 안에 끊김을 알고(종료 훅) 3~18초에 다시 붙음(연달아 재시작하면 간격이 1 → 16초로 늘어난다, 상한 30초), 통화 중 재시작은 두 기기 모두 통화 종료 → 약 3초 뒤 재접속. Bot Dashboard 등록(`tab5_phone_relay`, bot-dashboard 4b58203 — 대시보드의 재시작 버튼은 `restart relay`). **확인하지 않은 것**: 맥미니 재부팅 뒤 두 서비스가 스스로 뜨는지, 로그 회전.
- 확인하지 못한 것: 실제 launchd 등록(SSH 의 `gui/<uid>`)·`launchctl print` 의 `pid =` 모양은 가짜 명령으로만 시험했다(틀리면 스크립트가 안전하게 멈춘다). "DNS 이미 있음"이 이 터널을 가리키는지는 확인하지 못한다(`status` 의 https 200 으로 간접 확인). 로그 회전은 없다.

## 교환기 코드의 변화 (같은 묶음)

- 교환기가 꺼질 때 붙은 기기에 닫기 코드 1001 을 보낸다(P4N-D5). 통화 중이던 상대는 평소 이탈과 같은 `ended:lost` 를 받는다(새 신호 없음). 비용: 죽은 연결은 종료가 최대 2초 늦다. 탈출구: 훅 제거.
- 인증 실패 로그는 10초 창당 한 줄 + `(+N 생략)`(P4N-D6, `AUTH_LOG_WINDOW_S=0` 이면 예전처럼 매번). 로그에 토큰은 없다.
- **신호 규약 v2(2026-10-08)**: 위 "신호 규약 v2" 절. `phone_core.py` 는 통화를 기기 쌍마다의 객체로 든다(TW-D10), `phone_relay.py` 는 접속 헤더 `X-Phone-Proto` 와 환경변수 `PHONE_LEGACY_PAIR` 를 읽는다. `phone_relay_prod.sh` 는 바뀌지 않았다(기기 수를 전제한 줄이 없다 — 시험 `test_script_has_no_fixed_device_names_or_counts`). 도구: `phone_fake_peer.py --proto 2`·`send:call:<기기ID>`·`expect:ring:<기기ID>`·`expect:peer:<기기ID>:<on|off>`, `phone_smoke.py --three`, `phone_token_rotate.py --add-device`.
- 시험: 교환기 pytest 480(`cd server && python3 -m pytest -q`, ESP-IDF 를 export 하지 않은 셸에서). 신호 규약 v2 전에는 208.

## Bot Dashboard 등록 값 (등록은 설치 뒤)

이름 Tab5 전화 교환기 / 설명 Tab5 음성 전화기 WebSocket 중계(집↔사무실), 전용 터널 tab5phone / 포트 5094(127.0.0.1) / 상태 주소 `http://127.0.0.1:5094/healthz`(공개 `https://phone.example.com/healthz`) / launchd 라벨 위 둘 / 로그 위 두 파일 / 위치 맥미니 `~/tab5-toy-phone/server/phone_relay.py`.

## 되돌리기

- 교환기·터널 내리기: `phone_relay_prod.sh uninstall`(터널·DNS 는 안 지운다). 터널·DNS 까지 지우려면 출력된 안내대로 **UUID 로** `cloudflared tunnel delete` + Cloudflare 대시보드에서 DNS 삭제.
- 기기를 LAN 으로 되돌리기: `secrets.h` 의 `RELAY_URL` 을 `ws://…` 로(펌웨어 README "교환기 연결").
