# ProjectOverview — 프로젝트 목표와 기술 스택

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- 기존 Character·Framework·Manager·Component 아키텍처를 유지하며 P3-7 Anonymous Presentation PoC/B안 통합을 중단한다. 별도 실험 자료는 보존한다.
- 정상 게임 화면의 위장 일관성은 필수지만 네트워크 메타데이터 수준 정체 은폐는 필수 요구사항에서 제외한다. 비공개 게임 데이터 보호·Server Authority·최소 복제는 유지한다.
- C++ 기본값 Walk200/Sprint500, NPC 배율15 및 최대60, 플레이어 전용 이동 발소리 경로는 정적으로 확인했다. BP/DataAsset 실제 적용과 런타임은 미검증이다.
- P3-1~P3-5 최신 구현과 도착 제한 기본90초는 중앙 최신 절을 따른다.

## 1. 한 줄 소개

**Don't Melt**는 외형이 동일한 NPC 군중에 섞여 정체를 숨기고 유물을 쟁탈하는 **2~4인 온라인 개인전 심리전 게임**이다. 플레이어는 마녀의 저택과 젤리 마을을 돌아다니며 상대를 추리하고, 유물을 진영 상자에 등록해 승리한다.

## 2. 개발 목표

1. Unreal Engine 5.5 C++로 멀티플레이 전반(방 생성→로비→경기→결과→복귀)을 구현한다.
2. **Server Authority**: 승패·유물·피격·상태 전이·NPC/이벤트 진행을 서버만 확정한다.
3. **Single Source of Truth**: 게임 상태의 원본을 책임 객체 하나에 둔다.
4. **최소 정보 전송**: 정체 추리 게임에서 상대 식별·유물 관련 숨겨진 데이터가 네트워크로 새지 않게 한다.
5. **확장 가능한 표현**: C++는 조건·수명과 데이터, BP는 Widget/메시/애니메이션/Niagara/사운드 배치를 맡는다.
6. **다수 NPC 비용 통제**: AI는 타이머·콜백 중심, 이동·애니메이션·복제는 프로파일링 결과를 보고 조절한다.
7. 네트워크 비동기 실패와 맵 전환을 정상 수명주기의 일부로 다룬다.

## 3. 기술 스택

| 구분 | 채택 기술 / 설계 |
|---|---|
| 엔진 | **Unreal Engine 5.5** |
| 주 언어 | **C++** (Gameplay Framework, Actor/Component, GameMode/GameState, Subsystem, RPC) |
| 에디터 연동 | Blueprint, UMG/Widget Blueprint, Animation Blueprint, DataAsset, Niagara |
| 네트워크 | **Listen Server**, Server RPC / Client RPC / NetMulticast, RepNotify, OwnerOnly·필요 연결별 복제 |
| 온라인 | Steam OnlineSubsystem 및 Steam 세션, Steam 네트워크/음성 연동 (실제 플러그인·Config는 소스 확인 필요) |
| 입력 | Enhanced Input, `UCharacterMovementComponent`의 이동 예측 확장 (`FSavedMove`) |
| NPC | 공통 캐릭터 기반 클래스, 서버 AIController, Timer + Move 완료 이벤트, Nav 이동 |
| 표현 | Skeletal Mesh, `UDMJellyAnimInstance`, `ABP_DMJelly`, 사망 시 Ragdoll, 효과 및 UMG |
| 검증 | 정적 소스 대조, Automation Test(문서에 명시), PIE 멀티 및 Steam 패키징 회귀 시나리오 |

## 4. 프로젝트 규격(목표)

| 항목 | 최종 설계 문서 기준 |
|---|---|
| 인원 | 2~4인, 중도 참가/호스트 이전 미지원 |
| NPC 수 | `StartingPlayerCount × 15` → 30/45/60명; 경기 시작 때 스냅샷 고정 |
| 경기 | 정규 900초(15분), 필요 시 연장 180초(3분) |
| 리스폰 | 사망 시점부터 서버 기준 30초, 부활 보호 3초 |
| 유물 수 | 2인=3개, 3인=4개, 4인=5개 |
| 즉시 승리 등록 개수 | 2인=3개, 3인=3개, 4인=4개 |
| 맵 | Horror Mansion Map / 마녀의 저택·젤리 마을, BaseSlot 0~3과 중앙 공용 구역 |
| 기본 카메라 | 1인칭, 3인칭 전환 가능 |
| 시각 콘셉트 | 플레이어/NPC 외형을 동일하게 유지하는 젤리 캐릭터, 만화적 연출 |

> **코드 값 주의:** 기술 문서에는 과거 NPC 10배(최대 40명), WalkSpeed=300 등의 기록도 남아 있다. 별도 최신 사용자 지시로 WalkSpeed=200cm/s·SprintSpeed=500cm/s 변경 요청이 있었지만 현재 소스 반영 여부는 확인하지 못했다. 수치 충돌은 [ImplementationStatus](ImplementationStatus.md)에서 확인하고 현재 DataAsset/BP/C++ 기본값을 대조한다.

## 5. 게임플레이 핵심 루프

1. Steam 세션 생성 또는 검색·참가 → 로비 Ready/Host 검증.
2. 서버가 참가자·진영·NPC/유물 수를 확정하고 경기 맵으로 이동.
3. NPC와 같은 외형으로 이동·포즈·감정표현을 사용하며 정체를 숨김.
4. 필드 유물을 줍거나 상대 상자에서 탈취해 자기 상자에 등록.
5. 플레이어에 대한 공격·방어·스킬, 퀘스트/랜덤 이벤트 등으로 상대 행동을 유도.
6. 즉시 승리/정규 종료/연장전/중단 시 서버가 결과를 확정; 클라이언트 캐시 후 복귀.

## 6. 범위와 비목표

**포함 설계:** 세션·로비, Match Phase, 이동·자세·전투, 유물, 스킬, NPC, 퀘스트/이벤트, 사망 관전/리스폰, 결과, UI·설정, Steam Voice, 젤리 애니메이션 및 사망 Ragdoll, 장애 복구.

**포함하지 않음(기획상 명시적 제외):** Dedicated Server, 호스트 이전, 경기 중 중도 참가·재접속, Kill Replay(기록 재생), NPC 가짜 공격·일반 가짜 죽음, 퀘스트 완료 보상, 상시 전신 Active Ragdoll.

**최근 시각 작업 제안이나 구현 확인 전 항목:** 코 Static Mesh 부착, C++에서 트리거한 Niagara의 BP 배치, 오인 공격 느낌표 3D Static Mesh 연출. 목표 구현 방식은 [FeatureSpecs/16_Visual_Effects_And_Presentation.md](FeatureSpecs/16_Visual_Effects_And_Presentation.md)에 별도 정리했으며 완료로 간주하지 않는다.

## 7. 읽을 원본 문서

- [게임 기획서](../Docs/Design/GameDesign.md)
- [시스템 설계서](../Docs/Design/SystemArchitecture.md)
- [입력 설계](../Docs/Design/EnhancedInput.md)
- [UI 설계](../Docs/Design/UIDesign.md)
- [기술 문서 목차](../Docs/Technical/README.md)
- [최적화·측정](../Docs/Performance/README.md)

## 8. Codex에 요구하는 응답 계약

현재 구현에 대한 주장은 실제 파일·심볼·설정 위치를 근거로 제시한다. 사양과 다르면 `'설계: … / 소스: … / 조치: …'`로 분리한다. 테스트는 **실행함·실행 불가·미실행** 중 하나로 표기한다.
