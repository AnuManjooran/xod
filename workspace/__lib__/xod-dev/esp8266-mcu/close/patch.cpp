#pragma XOD evaluate_on_pin disable
#pragma XOD evaluate_on_pin enable input_CLS

struct State {
};

// clang-format off
{{ GENERATED_CODE }}
// clang-format on

void evaluate(Context ctx) {
    if (!isInputDirty<input_CLS>(ctx))
        return;

    auto client = getValue<input_SOCK>(ctx);
    client->stop();
    emitValue<output_DONE>(ctx, 1);
}
