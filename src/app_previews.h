#pragma once

namespace app {
bool FinishBuilderTransition();
bool PreparePreviews(const char* commandLine,bool benchmark,bool savedBuilderPreview,bool& finished);
bool RunImGuiReview();
bool RunStandardGuiReview();
}
