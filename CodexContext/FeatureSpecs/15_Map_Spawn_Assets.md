# 15. 맵·BaseSlot·스폰 포인트·에셋 계약

## 맵 콘셉트

마녀의 저택/젤리 마을을 중심으로 하는 `Horror Mansion Map`. 사탕·설탕 오브젝트, 진영별 유물 상자/시작점/리스폰 지점, 중앙 공용 유물 및 NPC 이동 동선이 필요하다. 정확한 `.umap` 파일명과 현재 배치는 첨부 파일에 없어 **확인 불가**.

## 필수 배치 계약

| 배치 객체 | 조건 |
|---|---|
| 진영 BaseSlot 0~3 | 최대 4인용 서로 구분되는 진영. 맵과 일치하는 슬롯 ID 보유 |
| `ADMBaseZone` + `ADMRelicChest` | 각 활성 BaseSlot에 유효한 상자·상호작용 지점 |
| 시작/리스폰 지점 | 참가자마다 매핑 가능한 BaseSlot 및 Character 생성 가능한 Capsule 공간 |
| `ADMRelicSpawnPoint` | 활성 진영 유물 각 1 + 중앙 공용 1, Drop 실패 복구 지점 |
| `ATargetPoint` tagged `DM_NPCSpawn` | 4인 최대 60명용 충분한 지점(동선·충돌·NavMesh 검증) |
| NavMesh / NPC 동선 | Walking/Sprint/RelicApproach/Flee/Panic으로 이동 가능한 구간 |
| `ADMBlackoutController` 및 조명 | 정전 이벤트에서 로컬 조명 연출·원상복귀 가능 |
| 공용 Skeletal Mesh/AnimBP/PhysicsAsset | 플레이어·NPC 동일 외형·젤리 연출·사망 물리 지원 |
| UI/입력 자산 | IMC_Gameplay/Pose/Interaction/SkillTargeting/Radial/Spectator/Voice, 필수 WBP 바인딩 |

**미확정:** 정확한 에셋 경로, 물리 소재, NPC 스폰 간격, 최종 라이팅/환경음·VFX 에셋은 현재 코드/Content 없이는 판단 불가. 존재하는 Blueprint 오버라이드와 실제 Skeleton Bone 이름을 확인해야 함.

## 규칙 연계

- 경기 시작 때 참가 인원·BaseSlot·Relic/SpawnPoint·NPC 목표 수를 검증. 일부 실패했는데 매치를 강행하면 장기 Waiting/유물 누락 발생 위험.
- NPC는 스폰 포인트 셔플. 시작 인원이 2명이라고 첫 30개 위치를 매번 그대로 쓰지 않음.
- 유물 떨어진 위치가 월드 바깥/복구 불가하면 미리 지정된 SpawnPoint로 반환. 임의 좌표 생성 금지.
- 리스폰 Spawn/Possess 실패하면 1초 간격 재시도; 정상 Alive 확정/클라이언트 Handoff 전제 아님.
- 상대 식별을 허용하지 않는 외형·UI: 상자 진영 문양은 서로 다른 것과 플레이어 외형을 연결하지 않음.

## 성능·검증

- 60 NPC + 4 Player 기준 AI/NavMesh·CharacterMovement·Animation·Voice·Actor Replication 측정.
- 스폰 위치 겹침, 시작 시 스냅샷 잠금, Physics Asset·Collision/Trace 채널, Pawn이 상호작용 LOS를 차단하지 않는지.
- BlendSpace/AnimNotify/PhysicsAsset 정상, 한글/영문 Widget 글꼴, 필수 액터가 없을 때 명확한 시작 실패 로그.

[게임 기획](../../Docs/Design/GameDesign.md) · [NPC](../../Docs/Technical/08_NPC_AI.md) · [패키징 체크리스트](../../Docs/Verification/02_Packaging_Checklist.md)
