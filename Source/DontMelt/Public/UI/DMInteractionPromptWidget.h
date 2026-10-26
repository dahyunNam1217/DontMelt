
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMInteractionPromptWidget.generated.h"

class AActor;
class UTextBlock;

/**
 * 상호작용 후보에 대한 F 입력 안내를 표시한다.
 * 실제 입력과 서버 요청은 기존 Character가 담당한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMInteractionPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Presentation

    void SetCandidate(AActor* InCandidate);

#pragma endregion

protected:
#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_InteractionPrompt = nullptr;

#pragma endregion
};
