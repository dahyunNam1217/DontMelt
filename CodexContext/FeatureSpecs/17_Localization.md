# 17. P9 현지화

**상태는 진행 중이다.** 기준 체크포인트에는 문화권 전환을 요청하는 C++와 현지화 설정·중간 산출물이 있다. 번역 적용, 실행 중 언어 전환, 패키징 및 스테이징은 검증 대기다. [기준 구현과 검증 상태](../ImplementationStatus.md#p1-p9-체크포인트-구현과-검증)

## 문화권 선택과 저장

`UDMLocalSettingsSubsystem`은 초기화에서 로컬 슬롯을 읽고 언어 코드를 적용한다. `SetLanguageCode`는 입력을 대소문자 구분 없이 `en` 또는 기본값 `ko`로 정규화한다. `FInternationalization::SetCurrentCulture`가 성공하면 메모리의 SaveGame 객체에 반영한다. 디스크에 쓰는 시점은 `UDMSettingsWidget::ApplyPendingChanges`가 `SaveSettings`를 호출할 때다. 디스크 저장 실패 경로에는 이전 문화권 복구가 있다. 세션 연결 실패 복구의 책임은 별도 설정 명세에 둔다.

[문화권·SaveGame 처리](../../Source/DontMelt/Private/Core/DMLocalSettingsSubsystem.cpp) · [설정 적용](../../Source/DontMelt/Private/UI/DMSettingsWidget.cpp) · [설정·장애 복구 명세](14_Settings_Network_Recovery.md)

## 문자열 식별과 확인 범위

사용자 표시 문자열은 `NSLOCTEXT`의 네임스페이스·키·원문과 같은 구조로 Gather에 들어간다. 같은 네임스페이스와 키를 다른 의미의 문자열에 재사용하면 충돌이 생길 수 있다. 기존 문자열을 바꿀 때는 키의 의미와 충돌 보고를 확인한다. 사용자 안내용 `FText`와 로그·내부 식별자는 역할을 구분한다.

기준의 충돌 로그에는 `DMFrontend / SessionUnavailable`에 서로 다른 원문이 기록됐다. 현재 참가 실패 분기에는 `JoinSessionUnavailable` 키가 있다. 충돌이 현재 Gather에서도 재현되는지, 변경으로 해소됐는지는 재실행으로 확인해야 한다. [Frontend 문구](../../Source/DontMelt/Private/Framework/Frontend/DMFrontendPlayerController.cpp) · [충돌 로그](../../Content/Localization/Game/Game_Conflicts.txt)

## Gather, Archive, Compile

| 단계 | 확인한 설정과 산출물 |
|---|---|
| Gather | `Game_Gather.ini`의 원본 문화권은 `ko`, 대상은 `en`과 `ko`다. `Source/DontMelt/`의 소스 필터와 `Content/_DontMelt/*`의 `.uasset`·`.umap` 패키지 필터를 사용한다. Engine과 Editor manifest 의존 경로는 Windows UE5.5 설치 구조 기준이다. |
| Manifest와 Archive | Gather 스텝은 `Game.manifest`, 문화권별 `Game.archive`, 단어 집계 `Game.csv`, 충돌 보고 `Game_Conflicts.txt`를 생성하도록 구성돼 있다. 이 중간 산출물은 `3eeb0d1`에 추적된다. CSV의 영어 단어 수 0은 당시 Gather 스냅샷이다. |
| Compile | `Game_Compile.ini`의 `GenerateTextLocalizationResource`는 manifest·archive를 읽어 `Game.locres`를 생성하도록 설정된다. 소스 일치 검사를 건너뛰지 않으며 포맷 패턴 검증이 켜져 있다. 기준 커밋에 추적된 `.locres`·`.locmeta`는 없다. |

[Gather 설정](../../Config/Localization/Game_Gather.ini) · [Compile 설정](../../Config/Localization/Game_Compile.ini) · [현지화 중간 산출물 커밋](https://github.com/dahyunNam1217/DontMelt/commit/3eeb0d1)

## 완료 판단에 필요한 증거

1. 대상 엔진에서 manifest 의존 경로를 확인하고 Gather를 재실행해 소스·BP 문자열 포함 범위와 키 충돌을 확인한다.
2. 번역과 포맷 인자의 대응, 문화권별 리소스 생성과 패키지 스테이징을 확인한다.
3. ko/en 전환·재실행 시 저장 복원, 이미 열려 있는 UI의 문구, 글꼴·줄바꿈을 에디터와 패키지에서 확인한다.

[로컬 설정 구현](../../Docs/Technical/15_Settings_NetworkRecovery.md) · [패키징 체크리스트](../../Docs/Verification/02_Packaging_Checklist.md)
