#include "UI/DMSpectatorCountdownWidget.h"

void UDMSpectatorCountdownWidget::SetCountdownValue(const int32 InCountdownValue)
{
    BP_SetCountdownValue(FMath::Clamp(InCountdownValue, 0, 5));
}