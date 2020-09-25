node {
    meta {
        struct Type {
            {{#each inputs}}
            {{#if (isConstantType type)}}static constexpr {{/if~}}
            typeof_{{ pinKey }} _{{ @index }}
            {{~#if (isConstantType type)}} = constant_input_{{ pinKey }}{{/if}};
            {{/each}}
        };
    }

    void evaluate(Context ctx) {
        {{#each outputs}}
        Type record = getValue<output_{{ pinKey }}>(ctx);
        {{/each}}

        {{#each inputs}}
        {{#unless (isConstantType type)}}
        record._{{ @index }} = getValue<input_{{ pinKey }}>(ctx);
        {{/unless}}
        {{/each}}

        {{#each outputs}}
        emitValue<output_{{ pinKey }}>(ctx, record);
        {{/each}}
    }
}
