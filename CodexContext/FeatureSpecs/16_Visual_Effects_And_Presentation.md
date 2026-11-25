# 16. 캐릭터 코·BP 조정형 이펙트·오인 공격 3D 느낌표

> **표현 설계와 구현 상태.** 제안은 기존 전투·스킬·NPC 판정에서 시각 표현을 분리하는 방향을 기록한다. 현재 C++에는 효과 부착·정리와 NPC 느낌표 훅이 있다. 아래 수치와 BP 이름은 설계 예시이며 메시 할당과 실제 재생은 미검증이다.

## 1. 별도 3D 코 부착

코 메시·소켓의 실제 캐릭터 BP 장착은 기준 커밋에서 미검증이다.

**목표:** 기존 젤리 Skeletal Mesh의 지오메트리/스키닝을 수정하지 않고, 별도 코 `Static Mesh`를 캐릭터 얼굴에 부착해 위치·회전·Scale을 Blueprint Viewport에서 조정.

**선호 구성:** 공용 캐릭터 Mesh의 실제 Head/얼굴 인접 Bone에 `NoseSocket`을 추가하거나 해당 본에 컴포넌트를 바로 부착 → 자식 `StaticMeshComponent`의 `Parent Socket`/Relative Transform에서 조절. Socket 추가는 Skeleton Asset 변경은 필요할 수 있으나 Skeletal Mesh 정점 편집은 불필요.

- 플레이어·NPC가 동일한 외형을 써야 하므로 **공유 베이스 또는 동일한 외형 구성 자산**에 적용.
- 코 충돌은 `NoCollision`, SimulatePhysics off. NPC 최대 60명에 따른 자원 비용/그림자·Material Slot 수 검토.
- 공용 Skeleton에서 해당 본이 실제 존재하는지 확인. 애니메이션의 본 회전은 따라가지만 Material 기반 WPO 변형까지 자동 추종하는 것은 아님.
- FBX Static Mesh 임포트 후 크기·축·머티리얼 검증. Unity 기반 `.unitypackage`의 스크립트·Shader는 이식 대상 아님.

## 2. BP에서 이펙트 위치·크기 편집

현재 `UDMCharacterVFXComponent`에 공격·방어·스턴 위치·회전 설정이 있다. 방어 Niagara의 부착과 정리 경로를 확인했다. 실제 BP 조정값과 Niagara 출력은 에디터 검증 대상이다.

**목표:** 이벤트 발생 조건은 기존 C++ 그대로 유지하고, 에셋 선택과 Relative Location/Rotation/Scale/Attach Socket/Duration을 BP에서 조절.

다음 중 **기존 소스 생성 방식과 호환되는 최소 수정안**을 택한다.

| 방식 | 채택 조건 | 장점/주의 |
|---|---|---|
| 캐릭터 BP `UNiagaraComponent` 사전 부착 | 자주 반복되는 같은 이펙트 | 에디터에서 위치 조정, Auto Activate off, 재생 시작/종료 관리 |
| C++ `BlueprintImplementableEvent` 연출 이벤트 | VFX/Static Mesh/사운드 교체가 빈번 | C++는 `Play...Presentation` 호출만, BP가 Spawn/Timeline/Attach 담당 |
| BP 노출 `TSubclassOf`/Asset/Offset 데이터 | 여러 스킨·에셋을 데이터로 바꿀 필요 | 선택 가능한 유형과 값만 노출; 무분별한 복제 금지 |

**주의:** 현재 C++에서 `SpawnSystemAtLocation`/`SpawnSystemAttached`로 직접 Spawn 중이라면 BP 컴포넌트를 단순 추가해도 자동으로 기존 경로가 바뀌지 않는다. 중복 VFX 생성이 생기지 않도록 기존 호출을 찾아 전환.

## 3. 오인 공격 시 3D 느낌표

`ADMNPCCharacter`에는 느낌표 StaticMesh와 표시·중단 BP 훅이 있다. 실제 메시 할당과 원격 표시 수명은 완료 기준에서 확인한다.

**목표:** NPC 오인 공격이 서버에서 확정된 뒤, 기존 VFX 대신/또는 대체하여 `Static Mesh` 느낌표가 지정 위치에 튀어나왔다 사라지는 Cosmetic 연출을 재생. **서버의 3초 스턴 및 NPC 반응 판정은 그대로 유지.**

| 항목 | 권장 제안(미확정) |
|---|---|
| 메시 | 코/캐릭터와 별도 `SM_Exclamation` |
| 위치 | 캐릭터 오른쪽 머리 위의 SceneComponent/Socket 부착 (화면 오른쪽 위 HUD라면 별도 UMG 설계 필요) |
| 활성화 | 기본 Hidden, 충돌 없음/물리 없음, 연출 발생 시 Visible |
| 타이밍 | `0.0s 0% → 0.12s 125% → 0.30s 100% → 0.65s 0%` 스케일 Timeline **예시** |
| 종료 | 숨김/재사용. 별도 Actor 매번 생성은 필요성 확인 후 판단 |
| 수신 대상 | 본인에게만인지, 주변 관찰자에게도 공개할지 **사용자에게 최종 결정 필요** (정체 정보 노출 가능) |

**서버/클라이언트 원칙:** 실제 오인 공격 확정은 서버 단일 경로. Client가 단순 Hit 이펙트 발생만으로 임의 3초 스턴 확정 금지. 다른 플레이어도 볼 연출이면 해당 연결과 Relevancy 정책을 정한 뒤 표현 이벤트 전송. 시각 아이콘이 인게임에서 공격자 식별 단서가 되는지 검토.

## 4. 시스템과 연결 경계

- `UDMCombatComponent`/NPC 반응 Director/`ADMMatchGameMode`가 기존 Gameplay 결과를 유지.
- 연출 이벤트는 **성공한 서버 Commit 이후** 호출하되, 기존 스턴/데스/상자/사운드 호출 경로와 중복되지 않게 한다.
- BP 에셋이 없을 때 Gameplay 판정 자체는 정상 동작. 연출만 조용히 생략 또는 안전한 Log.
- 사망/Travel/EndPlay 때 남은 Timeline·Component·Timer를 정리하고 새 Pawn에 이전 연출 참조를 유지하지 않는다.
- 장식 메시와 이펙트 모두 Pawn Collision/Visibility LOS를 차단하지 않아야 한다.

## 5. 완료 기준

1. 코 메시를 플레이어와 NPC에 동일하게 붙이고 Head Bone/Pose/점프/사망 시 뒤틀림을 확인.
2. Niagara 위치·Scale·Socket을 BP 에디터에서 조정하면 **C++ 재컴파일 없이** 에셋 편집만으로 조절 가능.
3. 오인 공격 Server 확정 후 느낌표 한 번만 재생, 연속 공격·중단·리스폰·멀티에서 중복 표시나 영구 잔존 없음.
4. 4인/60 NPC에서 Static Mesh·Niagara Component·Material 비용 측정, 은폐 정보 UI 정책과 충돌하지 않음.
5. 실제로 어느 클라이언트에 보여야 하는지 결정 및 테스트 항목에 기록.

관련 근거: [전투 설계](../../Docs/Technical/07_Combat_Death.md) · [젤리 애니메이션](../../Docs/Design/Stages/11_5_JellyAnimationAndRagdoll.md) · [UI 정책](../../Docs/Design/UIDesign.md). 신규 시각 표현 설계 자체는 기존 파일이 아닌 최근 사용자 요구를 바탕으로 작성했다.
