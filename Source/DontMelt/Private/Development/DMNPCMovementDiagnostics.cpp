#include "Development/DMNPCMovementDiagnostics.h"

#if WITH_EDITOR
#include "Characters/DMNPCCharacter.h"
#include "Characters/DMJellyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMNPCDiagnostics, Log, All);

namespace
{
    struct FObservation
    {
        FVector Anchor = FVector::ZeroVector;
        FVector Goal = FVector::ZeroVector;
        double AnchorTime = 0;
        float MovementDelta = 0;
        uint64 MovementFrame = 0;
        uint32 Request = 0;
        bool bHadIntent = false;
        TWeakObjectPtr<UPathFollowingComponent> Path;
        FDelegateHandle CompletionHandle;
    };
    bool bDiagnosticsEnabled = false;
    FString OutputPath;
    FString Pending;
    TMap<TWeakObjectPtr<UWorld>, FTimerHandle> Timers;
    TMap<TWeakObjectPtr<ADMNPCCharacter>, FObservation> Observations;
    FDelegateHandle InitHandle;
    FDelegateHandle CleanupHandle;

    bool IsTestWorld(const UWorld* World)
    {
        return World && World->WorldType == EWorldType::PIE
            && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_Client)
            && UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) == TEXT("/Game/_DontMelt/Maps/Test/MatchMap_T");
    }
    void Flush()
    {
        if (!Pending.IsEmpty())
        {
            FFileHelper::SaveStringToFile(Pending, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                &IFileManager::Get(), FILEWRITE_Append);
            Pending.Reset();
        }
    }
    const TCHAR* BehaviorName(EDMNPCBehaviorState State)
    {
        switch (State)
        {
        case EDMNPCBehaviorState::Idle: return TEXT("Idle");
        case EDMNPCBehaviorState::Waiting: return TEXT("Waiting");
        case EDMNPCBehaviorState::Moving: return TEXT("Moving");
        case EDMNPCBehaviorState::RelicApproachMoving: return TEXT("RelicApproachMoving");
        case EDMNPCBehaviorState::Reacting: return TEXT("Reacting");
        case EDMNPCBehaviorState::FleeMoving: return TEXT("FleeMoving");
        case EDMNPCBehaviorState::PanicWaiting: return TEXT("PanicWaiting");
        case EDMNPCBehaviorState::PanicMoving: return TEXT("PanicMoving");
        case EDMNPCBehaviorState::DeathMocking: return TEXT("DeathMocking");
        default: return TEXT("Unknown");
        }
    }
    void Row(const TCHAR* Type, ADMNPCCharacter* NPC, float Delta, const FString& Detail)
    {
        if (!bDiagnosticsEnabled || !IsValid(NPC) || !IsTestWorld(NPC->GetWorld())) return;
        UWorld* World = NPC->GetWorld();
        Pending += FString::Printf(TEXT("%s\t%llu\t%s\t%s\t%d\t%.4f\t%.5f\t%.5f\t%s\t%s\t%s\n"),
            Type, GFrameCounter, *World->GetName(), *NPC->GetName(), static_cast<int32>(NPC->GetLocalRole()),
            World->GetTimeSeconds(), World->GetDeltaSeconds(), Delta, *NPC->GetActorLocation().ToCompactString(),
            *NPC->GetVelocity().ToCompactString(), *Detail);
    }
    void Sample(TWeakObjectPtr<UWorld> WeakWorld)
    {
        UWorld* World = WeakWorld.Get();
        if (!bDiagnosticsEnabled || !IsTestWorld(World)) return;
        for (TActorIterator<ADMNPCCharacter> It(World); It; ++It)
        {
            ADMNPCCharacter* NPC = *It;
            FObservation& O = Observations.FindOrAdd(NPC);
            ADMJellyAIController* AI = Cast<ADMJellyAIController>(NPC->GetController());
            UPathFollowingComponent* Path = AI ? AI->GetPathFollowingComponent() : nullptr;
            const bool bMoving = Path && Path->GetStatus() == EPathFollowingStatus::Moving;
            const FVector Goal = bMoving ? Path->GetPathDestination() : FVector::ZeroVector;
            const uint32 Request = Path ? Path->GetCurrentRequestId().GetID() : 0;
            // Idle/목적지 도착은 정체로 세지 않는다. 새 요청과 10cm 이상 전진 시 관찰 기준을 갱신한다.
            const bool bIntent = bMoving && FVector::Dist2D(NPC->GetActorLocation(), Goal) > 100.f;
            if (!bIntent || !O.bHadIntent || Request != O.Request || FVector::DistSquared2D(O.Anchor, NPC->GetActorLocation()) >= 100.f)
            {
                O.Anchor = NPC->GetActorLocation();
                O.AnchorTime = World->GetTimeSeconds();
            }
            O.bHadIntent = bIntent;
            O.Request = Request;
            O.Goal = Goal;
            const double NoProgress = bIntent ? World->GetTimeSeconds() - O.AnchorTime : 0;
            bool bGoalOnNav = false;
            if (bMoving)
            {
                FNavLocation Projected;
                UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
                bGoalOnNav = Nav && Nav->ProjectPointToNavigation(Goal, Projected, FVector(10, 10, 100))
                    && FVector::DistSquared2D(Goal, Projected.Location) < 100.f;
            }
            if (Path && !O.CompletionHandle.IsValid())
            {
                O.Path = Path;
                const TWeakObjectPtr<ADMNPCCharacter> WeakNPC = NPC;
                O.CompletionHandle = Path->OnRequestFinished.AddLambda([WeakNPC](FAIRequestID ID, const FPathFollowingResult& Result)
                {
                    Row(TEXT("PathFinished"), WeakNPC.Get(), 0, FString::Printf(TEXT("Request=%u Result=%d"), ID.GetID(), static_cast<int32>(Result.Code)));
                });
            }
            Row(TEXT("Sample"), NPC, O.MovementDelta, FString::Printf(TEXT("AI=%s Path=%s Request=%u Goal=%s GoalOnNav=%d Intent=%d NoProgress=%.2f Candidate=%d MoveFrame=%llu Mode=%s"),
                AI ? BehaviorName(AI->GetDiagnosticBehaviorState()) : TEXT("None"), Path ? *Path->GetStatusDesc() : TEXT("None"),
                Request, *Goal.ToCompactString(), bGoalOnNav, bIntent, NoProgress, NoProgress >= 3.0, O.MovementFrame,
                *NPC->GetCharacterMovement()->GetMovementName()));
        }
        Flush();
    }
    void AttachWorld(UWorld* World)
    {
        if (!bDiagnosticsEnabled || !IsTestWorld(World) || Timers.Contains(World)) return;
        const TWeakObjectPtr<UWorld> WeakWorld = World;
        FTimerHandle Timer;
        World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakWorld]() { Sample(WeakWorld); }), 1.f, true);
        Timers.Add(World, Timer);
    }
    void DetachWorld(UWorld* World)
    {
        if (FTimerHandle* Timer = Timers.Find(World)) World->GetTimerManager().ClearTimer(*Timer);
        Timers.Remove(World);
        for (auto It = Observations.CreateIterator(); It; ++It)
        {
            ADMNPCCharacter* NPC = It.Key().Get();
            if (!NPC || NPC->GetWorld() == World)
            {
                if (UPathFollowingComponent* Path = It.Value().Path.Get()) Path->OnRequestFinished.Remove(It.Value().CompletionHandle);
                It.RemoveCurrent();
            }
        }
        Flush();
    }
    void Stop()
    {
        bDiagnosticsEnabled = false;
        TArray<TWeakObjectPtr<UWorld>> Worlds;
        Timers.GenerateKeyArray(Worlds);
        for (auto World : Worlds) if (World.IsValid()) DetachWorld(World.Get());
        Timers.Empty();
        Observations.Empty();
        FWorldDelegates::OnPostWorldInitialization.Remove(InitHandle);
        FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
        InitHandle.Reset();
        CleanupHandle.Reset();
        Flush();
        UE_LOG(LogDMNPCDiagnostics, Log, TEXT("NPC diagnostic stopped. Timers/delegates cleared. File=%s"), *OutputPath);
    }
    void Start(const TArray<FString>& Args)
    {
        Stop();
        const FString Tag = FPaths::MakeValidFileName(Args.Num() ? Args[0] : TEXT("Run"));
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("P3_11f"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        OutputPath = FPaths::Combine(Directory, FString::Printf(TEXT("NPCDiag_%s_%s.tsv"), *Tag, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
        Pending = TEXT("Type\tFrame\tWorld\tNPC\tRole\tWorldTime\tWorldDt\tMoveDt\tLocation\tVelocity\tDetail\n");
        bDiagnosticsEnabled = true;
        InitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld* W, const UWorld::InitializationValues) { AttachWorld(W); });
        CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* W, bool, bool)
        {
            if (!Timers.Contains(W)) return;
            DetachWorld(W);
            if (Timers.IsEmpty()) Stop();
        });
        for (const FWorldContext& Context : GEngine->GetWorldContexts()) AttachWorld(Context.World());
        UE_LOG(LogDMNPCDiagnostics, Log, TEXT("NPC diagnostic armed. PIE only, sample=1s, no gameplay changes. File=%s"), *OutputPath);
    }
    FAutoConsoleCommand StartCommand(TEXT("DM.NPCDiag.Start"), TEXT("PIE NPC 관찰 시작. 선택 인자: 실행 태그."), FConsoleCommandWithArgsDelegate::CreateStatic(&Start));
    FAutoConsoleCommand StopCommand(TEXT("DM.NPCDiag.Stop"), TEXT("PIE NPC 관찰과 임시 Timer/Delegate 정리."), FConsoleCommandDelegate::CreateStatic(&Stop));
}

namespace DMNPCMovementDiagnostics
{
    void RecordMovement(UCharacterMovementComponent* Movement, float DeltaSeconds)
    {
        if (!bDiagnosticsEnabled) return;
        ADMNPCCharacter* NPC = Movement ? Cast<ADMNPCCharacter>(Movement->GetOwner()) : nullptr;
        if (!NPC || !IsTestWorld(NPC->GetWorld())) return;
        AttachWorld(NPC->GetWorld());
        FObservation& O = Observations.FindOrAdd(NPC);
        O.MovementDelta = DeltaSeconds;
        O.MovementFrame = GFrameCounter;
        // 느린 이동 update만 별도 기록한다. 경고의 RemainingTime과 실제 입력 Delta를 구분한다.
        if (DeltaSeconds > 0.05f) Row(TEXT("LongMovementUpdate"), NPC, DeltaSeconds, Movement->GetMovementName());
    }
    void RecordStuck(UCharacterMovementComponent* Movement, const FHitResult* Hit)
    {
        if (!bDiagnosticsEnabled) return;
        ADMNPCCharacter* NPC = Movement ? Cast<ADMNPCCharacter>(Movement->GetOwner()) : nullptr;
        if (!NPC || !IsTestWorld(NPC->GetWorld())) return;
        Row(TEXT("Stuck"), NPC, 0, FString::Printf(TEXT("Actor=%s Component=%s Depth=%.3f Mode=%s"),
            Hit ? *GetNameSafe(Hit->GetActor()) : TEXT("None"), Hit ? *GetNameSafe(Hit->GetComponent()) : TEXT("None"),
            Hit ? Hit->PenetrationDepth : 0, *Movement->GetMovementName()));
    }
}
#endif
