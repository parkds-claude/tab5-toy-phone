# Execution Plans

실행 계획을 **git 1급 아티팩트**로 보관하는 디렉터리.
에이전트가 외부 상황에 의존하지 않고 작업을 이어갈 수 있도록 한다.

## 계획 유형

| 유형 | 위치 | 수명 |
|---|---|---|
| 일시적 계획 (Ephemeral) | PR 설명 또는 `docs/daily/` | 병합 후 폐기 |
| 실행 계획 (Execution Plan) | `docs/exec-plans/<slug>.md` | 작업 완료까지 |
| 아카이브 | `docs/exec-plans/_archive/<slug>.md` | 영구 보존 |

## 작성 규칙

1. 비단순 작업(2단계 이상, 1일 이상 추정)은 **무조건** 실행 계획부터 작성.
2. 파일명은 `YYYY-MM-DD-<kebab-slug>.md` (예: `2026-05-22-layer-check-rollout.md`).
3. `_template.md`를 복제해서 작성.
4. 작업 진행에 따라 체크박스 갱신, 가설이 깨지면 즉시 수정.
5. 완료 후 `_archive/` 로 이동 (삭제 금지 — 향후 회고·디버깅 근거).

## 필수 필드

- 목표(Goal)
- 비목표(Non-Goals) — 의도적으로 안 하는 것
- 영향 범위(Blast Radius)
- 단계별 산출물(Steps)
- 검증 기준(Acceptance)
- 롤백 절차(Rollback)

## 운영

- 이 디렉터리는 에이전트가 항상 먼저 읽는다.
- 진행 중 계획이 있으면 새로운 계획을 시작하기 전에 충돌 여부를 점검.
- 30일 이상 갱신 안 된 계획은 자동 알림(향후 doc-gardening 봇이 처리).
