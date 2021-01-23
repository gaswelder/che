<?php

function parse_module($lexer)
{
    $m = new c_module;
    while ($lexer->more()) {
        $m->elements[] = parse_module_element($lexer);
    }
    return $m;
}

function parse_module_element($lexer)
{
    switch ($lexer->peek()['kind']) {
        case 'import':
            $import = parse_import($lexer);
            $m = resolve_import($import);
            $lexer->typenames = array_merge($lexer->typenames, $m->types());
            return $import;
        case 'typedef':
            return parse_typedef($lexer);
        case 'macro':
            return parse_compat_macro($lexer);
    }

    $pub = false;
    if ($lexer->follows('pub')) {
        $lexer->get();
        $pub = true;
    }
    if ($lexer->follows('enum')) {
        return parse_enum($lexer, $pub);
    }
    try {
        $type = parse_type($lexer);
    } catch (Exception $e) {
        throw new Exception("unexpected input (expecting function, variable, typedef, struct, enum) || " . $e->getMessage());
    }
    $form = parse_form($lexer);
    if ($lexer->peek()['kind'] == '(') {
        $parameters = parse_function_parameters($lexer);
        $body = parse_body($lexer);
        return new c_function_declaration($pub, $type, $form, $parameters, $body);
    }

    if ($lexer->peek()['kind'] != '=') {
        throw new Exception("module variable: '=' expected");
    }

    if ($lexer->peek()['kind'] == '=') {
        if ($pub) {
            throw new Exception("module variables can't be exported");
        }
        $lexer->get();
        $value = parse_expression($lexer);
        expect($lexer, ';', 'module variable declaration');
        return new c_module_variable($type, $form, $value);
    }
    throw new Exception("unexpected input");
}

function parse_anonymous_parameters($lexer)
{
    $node = new c_anonymous_parameters;
    expect($lexer, '(', 'anonymous function parameters');
    if (!$lexer->follows(')')) {
        $node->forms[] = parse_anonymous_typeform($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->forms[] = parse_anonymous_typeform($lexer);
        }
    }
    expect($lexer, ')', 'anonymous function parameters');
    return $node;
}

function parse_import($lexer)
{
    expect($lexer, 'import');
    $tok = expect($lexer, 'string');
    // expect($lexer, ';');
    $node = new c_import;
    $node->path = $tok['content'];
    return $node;
}

function parse_typedef($lexer)
{
    $node = new c_typedef;

    expect($lexer, 'typedef');

    if ($lexer->follows('{')) {
        $node->type = parse_composite_type($lexer);
        $node->alias = parse_identifier($lexer);
        expect($lexer, ';', 'typedef');
        return $node;
    }

    $node->type = parse_type($lexer, 'typedef');
    while ($lexer->follows('*')) {
        $node->before .= $lexer->get()['kind'];
    }
    $node->alias = parse_identifier($lexer);

    if ($lexer->follows('(')) {
        $node->after .= format_node(parse_anonymous_parameters($lexer));
    }

    if ($lexer->follows('[')) {
        $lexer->get();
        $node->after .= '[';
        $node->after .= expect($lexer, 'num')['content'];
        expect($lexer, ']');
        $node->after .= ']';
    }

    expect($lexer, ';', 'typedef');
    return $node;
}

function parse_statement($lexer)
{
    $next = $lexer->peek();
    if (
        ($next['kind'] == 'word' && is_type($next['content'], $lexer->typenames))
        || $next['kind'] == 'const'
    ) {
        return parse_variable_declaration($lexer);
    }

    switch ($next['kind']) {
        case 'if':
            return parse_if($lexer);
        case 'for':
            return parse_for($lexer);
        case 'while':
            return parse_while($lexer);
        case 'defer':
            return parse_defer($lexer);
        case 'return':
            return parse_return($lexer);
        case 'switch':
            return parse_switch($lexer);
    }

    $expr = parse_expression($lexer);
    expect($lexer, ';', 'parsing statement');
    return $expr;
}

function parse_expression($lexer, $current_strength = 0)
{
    $result = parse_atom($lexer);
    while (is_op($lexer->peek()['kind'])) {
        // If the operator is not stronger that our current level,
        // yield the result.
        if (operator_strength($lexer->peek()['kind']) <= $current_strength) {
            return $result;
        }
        $op = $lexer->get()['kind'];
        $next = parse_expression($lexer, operator_strength($op));
        $result = new c_binary_op($op, $result, $next);
    }
    return $result;
}

function parse_atom($lexer)
{
    $nono = ['case', 'default', 'if', 'else', 'for', 'while', 'switch'];
    if (in_array($lexer->peek()['kind'], $nono)) {
        throw new Exception("expression: unexpected keyword '" . $lexer->peek()['kind'] . "'");
    }

    if (
        $lexer->peek()['kind'] == '('
        && $lexer->peek_n(1)['kind'] == 'word'
        && is_type($lexer->peek_n(1)['content'], $lexer->typenames)
    ) {
        expect($lexer, '(');
        $typeform = parse_anonymous_typeform($lexer);
        expect($lexer, ')', 'typecast');
        return new c_cast($typeform, parse_expression($lexer));
    }

    if ($lexer->peek()['kind'] == '(') {
        $lexer->get();
        $expr = parse_expression($lexer);
        expect($lexer, ')');
        return $expr;
    }

    if ($lexer->peek()['kind'] == '{') {
        if ($lexer->peek_n(1)['kind'] == '.') {
            return parse_struct_literal($lexer);
        }
        return parse_array_literal($lexer);
    }

    if ($lexer->peek()['kind'] == 'sizeof') {
        return parse_sizeof($lexer);
    }

    if (is_prefix_op($lexer->peek()['kind'])) {
        $op = $lexer->get()['kind'];
        return new c_prefix_operator($op, parse_expression($lexer, operator_strength('prefix')));
    }

    if ($lexer->peek()['kind'] == 'word') {
        $result = parse_identifier($lexer);
    } else {
        $result = parse_literal($lexer);
    }

    while ($lexer->more()) {
        if ($lexer->peek()['kind'] == '(') {
            $args = parse_function_arguments($lexer);
            $result = new c_function_call($result, $args);
            continue;
        }
        if ($lexer->peek()['kind'] == '[') {
            expect($lexer, '[', 'array index');
            $index = parse_expression($lexer);
            expect($lexer, ']', 'array index');
            $result = new c_array_index($result, $index);
            continue;
        }

        if (is_postfix_op($lexer->peek()['kind'])) {
            $op = $lexer->get()['kind'];
            $result = new c_postfix_operator($result, $op);
            continue;
        }
        break;
    }

    return $result;
}

function with_comment($comment, $message)
{
    if ($comment) {
        return $comment . ': ' . $message;
    }
    return $message;
}

function expect($lexer, $type, $comment = null)
{
    if (!$lexer->more()) {
        throw new Exception(with_comment($comment, "expected '$type', got end of file"));
    }
    $next = $lexer->peek();
    if ($next['kind'] != $type) {
        throw new Exception(with_comment($comment, "expected '$type', got $next at $next[pos]"));
    }
    return $lexer->get();
}


function is_type($name, $typenames)
{
    return call_rust_mem('is_type', $name, $typenames);
}

function is_op($token_type)
{
    return call_rust_mem("is_op", $token_type);
}

function is_prefix_op($op)
{
    return call_rust_mem("is_prefix_op", $op);
}

function is_postfix_op($op)
{
    return call_rust_mem("is_postfix_op", $op);
}

function operator_strength($op)
{
    return call_rust_mem("operator_strength", $op);
}

function parse_array_literal_member($lexer)
{
    if ($lexer->follows('{')) {
        return parse_array_literal($lexer);
    }
    if ($lexer->follows('word')) {
        return parse_identifier($lexer);
    }
    return parse_literal($lexer);
}

function parse_array_literal_entry($lexer)
{
    $node = new c_array_literal_entry;
    if ($lexer->follows('[')) {
        $lexer->get();
        if ($lexer->follows('word')) {
            $node->index = parse_identifier($lexer);
        } else {
            $node->index = parse_literal($lexer);
        }
        expect($lexer, ']', 'array literal entry');
        expect($lexer, '=', 'array literal entry');
    }
    $node->value = parse_array_literal_member($lexer);
    return $node;
}

function parse_array_literal($lexer)
{
    $node = new c_array_literal;
    expect($lexer, '{', 'array literal');
    if (!$lexer->follows('}')) {
        $node->values[] = parse_array_literal_entry($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->values[] = parse_array_literal_entry($lexer);
        }
    }
    expect($lexer, '}', 'array literal');
    return $node;
}

function parse_body($lexer)
{
    $node = new c_body;
    if ($lexer->follows('{')) {
        expect($lexer, '{');
        while (!$lexer->follows('}')) {
            $node->statements[] = parse_statement($lexer);
        }
        expect($lexer, '}');
    } else {
        $node->statements[] = parse_statement($lexer);
    }
    return $node;
}

function parse_compat_macro($lexer)
{
    $node = new c_compat_macro;
    $node->content = expect($lexer, 'macro')['content'];
    return $node;
}

function parse_composite_type($lexer)
{
    $node = new c_composite_type;
    expect($lexer, '{', 'struct type definition');
    while ($lexer->more() && $lexer->peek()['kind'] != '}') {
        if ($lexer->peek()['kind'] == 'union') {
            $node->fieldlists[] = parse_union($lexer);
        } else {
            $node->fieldlists[] = parse_struct_fieldlist($lexer);
        }
    }
    expect($lexer, '}');
    return $node;
}

function parse_defer($lexer)
{
    expect($lexer, 'defer');
    $node = new c_defer;
    $node->expression = parse_expression($lexer);
    expect($lexer, ';');
    return $node;
}

function parse_union($lexer)
{
    $node = new c_union;
    expect($lexer, 'union');
    expect($lexer, '{');
    while (!$lexer->follows('}')) {
        $node->fields[] = parse_union_field($lexer);
    }
    expect($lexer, '}');
    $node->form = parse_form($lexer);
    expect($lexer, ';');
    return $node;
}

function parse_enum_member($lexer)
{
    $node = new c_enum_member;
    $node->id = parse_identifier($lexer, 'enum member');
    if ($lexer->follows('=')) {
        $lexer->get();
        $node->value = parse_literal($lexer, 'enum member');
    }
    return $node;
}

function parse_enum($lexer, $pub)
{
    $node = new c_enum;
    $node->pub = $pub;
    expect($lexer, 'enum', 'enum definition');
    expect($lexer, '{', 'enum definition');
    $node->members[] = parse_enum_member($lexer);
    while ($lexer->follows(',')) {
        $lexer->get();
        $node->members[] = parse_enum_member($lexer);
    }
    expect($lexer, '}', 'enum definition');
    expect($lexer, ';', 'enum definition');
    return $node;
}

function parse_for($lexer)
{
    $node = new c_for;
    expect($lexer, 'for');
    expect($lexer, '(');

    if ($lexer->peek()['kind'] == 'word' && is_type($lexer->peek()['content'], $lexer->typenames)) {
        $node->init = parse_loop_counter_declaration($lexer);
    } else {
        $node->init = parse_expression($lexer);
    }

    expect($lexer, ';');
    $node->condition = parse_expression($lexer);
    expect($lexer, ';');
    $node->action = parse_expression($lexer);
    expect($lexer, ')');
    $node->body = parse_body($lexer);
    return $node;
}

function parse_loop_counter_declaration($lexer)
{
    $node = new c_loop_counter_declaration;
    $node->type = parse_type($lexer);
    $node->name = parse_identifier($lexer);
    expect($lexer, '=');
    $node->value = parse_expression($lexer);
    return $node;
}

function parse_type($lexer, $comment = null)
{
    $node = new c_type;

    if ($lexer->follows('const')) {
        $lexer->get();
        $node->const = true;
    }

    if ($lexer->follows('struct')) {
        $lexer->get();
        $name = expect($lexer, 'word')['content'];
        $node->type = 'struct ' . $name;
    } else {
        $tok = expect($lexer, 'word', $comment);
        $node->type = $tok['content'];
    }

    return $node;
}

function parse_function_parameter($lexer)
{
    $node = new c_function_parameter;
    $node->type = parse_type($lexer);
    $node->forms[] = parse_form($lexer);
    while (
        $lexer->follows(',')
        && $lexer->peek_n(1)['kind'] != '...'
        && $lexer->peek_n(1)['kind'] != 'const'
        && !($lexer->peek_n(1)['kind'] == 'word' && is_type($lexer->peek_n(1)['content'], $lexer->typenames))
    ) {
        $lexer->get();
        $node->forms[] = parse_form($lexer);
    }
    return $node;
}

function parse_function_parameters($lexer)
{
    $node = new c_function_parameters;

    expect($lexer, '(');
    if (!$lexer->follows(')')) {
        $node->parameters[] = parse_function_parameter($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            if ($lexer->follows('...')) {
                $lexer->get();
                $node->parameters[] = new c_ellipsis();
                break;
            }
            $node->parameters[] = parse_function_parameter($lexer);
        }
    }
    expect($lexer, ')');
    return $node;
}

function parse_form($lexer)
{
    // *argv[]
    $node = new c_form;
    while ($lexer->follows('*')) {
        $node->str .= $lexer->get()['kind'];
    }
    $node->str .= expect($lexer, 'word')['content'];

    while ($lexer->follows('[')) {
        $node->str .= $lexer->get()['kind'];
        while ($lexer->more() && $lexer->peek()['kind'] != ']') {
            $expr = parse_expression($lexer);
            $node->str .= format_node($expr);
        }
        expect($lexer, ']');
        $node->str .= ']';
    }
    return $node;
}

function parse_switch($lexer)
{
    $node = new c_switch;
    expect($lexer, 'switch');
    expect($lexer, '(');
    $node->value = parse_expression($lexer);
    expect($lexer, ')');
    expect($lexer, '{');
    while ($lexer->follows('case')) {
        $node->cases[] = parse_switch_case($lexer);
    }
    if ($lexer->follows('default')) {
        $node->default = parse_switch_default($lexer);
    }
    expect($lexer, '}');
    return $node;
}

function parse_switch_case($lexer)
{
    $node = new c_switch_case;
    expect($lexer, 'case');
    if ($lexer->follows('word')) {
        $node->value = parse_identifier($lexer);
    } else {
        $node->value = parse_literal($lexer);
    }
    expect($lexer, ':');
    $until = ['case', 'break', 'default', '}'];
    while ($lexer->more() && !in_array($lexer->peek()['kind'], $until)) {
        $node->statements[] = parse_statement($lexer);
    }
    return $node;
}

function parse_literal($lexer)
{
    $types = ['string', 'num', 'char'];
    $node = new c_literal;
    foreach ($types as $type) {
        if ($lexer->peek()['kind'] != $type) {
            continue;
        }
        $node->value = $lexer->get()['content'];
        $node->type = $type;
        return $node;
    }
    if ($lexer->peek()['kind'] == 'word' && $lexer->peek()['content'] == 'NULL') {
        $lexer->get();
        $node->type = 'null';
        $node->value = 'NULL';
        return $node;
    }
    throw new Exception("literal expected, got " . $lexer->peek());
}

function parse_struct_literal_member($lexer)
{
    $node = new c_struct_literal_member;
    expect($lexer, '.', 'struct literal member');
    $node->name = parse_identifier($lexer, 'struct literal member');
    expect($lexer, '=', 'struct literal member');
    $node->value = parse_expression($lexer, 'struct literal member');
    return $node;
}

function parse_identifier($lexer, $hint = null)
{
    $tok = expect($lexer, 'word', $hint);
    $node = new c_identifier;
    $node->name = $tok['content'];
    return $node;
}

function parse_struct_fieldlist($lexer)
{
    $node = new c_struct_fieldlist;
    if ($lexer->follows('struct')) {
        throw new Exception("can't parse nested structs, please consider a typedef");
    }
    $node->type = parse_type($lexer);
    $node->forms[] = parse_form($lexer);

    while ($lexer->follows(',')) {
        $lexer->get();
        $node->forms[] = parse_form($lexer);
    }

    expect($lexer, ';');
    return $node;
}

function parse_if($lexer)
{
    $node = new c_if;
    expect($lexer, 'if', 'if statement');
    expect($lexer, '(', 'if statement');
    $node->condition = parse_expression($lexer);
    expect($lexer, ')', 'if statement');
    $node->body = parse_body($lexer);
    if ($lexer->follows('else')) {
        $lexer->get();
        $node->else = parse_body($lexer);
    }
    return $node;
}

function parse_struct_literal($lexer)
{
    $node = new c_struct_literal;
    expect($lexer, '{', 'struct literal');
    $node->members[] = parse_struct_literal_member($lexer);
    while ($lexer->follows(',')) {
        $lexer->get();
        $node->members[] = parse_struct_literal_member($lexer);
    }
    expect($lexer, '}', 'struct literal');
    return $node;
}


function parse_anonymous_typeform($lexer)
{
    $node = new c_anonymous_typeform;
    $node->type = parse_type($lexer);
    while ($lexer->follows('*')) {
        $node->ops[] = $lexer->get()['kind'];
    }
    return $node;
}

function parse_variable_declaration($lexer)
{
    $node = new c_variable_declaration;
    $node->type = parse_type($lexer);

    $node->forms[] = parse_form($lexer);
    expect($lexer, '=', 'variable declaration');
    $node->values[] = parse_expression($lexer);

    while ($lexer->follows(',')) {
        $lexer->get();
        $node->forms[] = parse_form($lexer);
        expect($lexer, '=', 'variable declaration');
        $node->values[] = parse_expression($lexer);
    }

    expect($lexer, ';');
    return $node;
}

function parse_return($lexer)
{
    $node = new c_return;
    expect($lexer, 'return');
    if ($lexer->peek()['kind'] == ';') {
        $lexer->get();
        return $node;
    }
    $node->expression = parse_expression($lexer);
    expect($lexer, ';');
    return $node;
}

function parse_while($lexer)
{
    $node = new c_while;
    expect($lexer, 'while');
    expect($lexer, '(');
    $node->condition = parse_expression($lexer);
    expect($lexer, ')');
    $node->body = parse_body($lexer);
    return $node;
}

function parse_sizeof($lexer)
{
    $node = new c_sizeof;
    expect($lexer, 'sizeof');
    expect($lexer, '(');
    if ($lexer->peek()['kind'] == 'word' && is_type($lexer->peek()['content'], $lexer->typenames)) {
        $node->argument = parse_type($lexer);
    } else {
        $node->argument = parse_expression($lexer);
    }
    expect($lexer, ')');
    return $node;
}

function parse_function_arguments($lexer)
{
    $node = new c_function_arguments;
    expect($lexer, '(');
    if ($lexer->more() && $lexer->peek()['kind'] != ')') {
        $node->arguments[] = parse_expression($lexer);
        while ($lexer->follows(',')) {
            $lexer->get();
            $node->arguments[] = parse_expression($lexer);
        }
    }
    expect($lexer, ')');
    return $node;
}

function parse_union_field($lexer)
{
    $node = new c_union_field;
    $node->type = parse_type($lexer);
    $node->form = parse_form($lexer);
    expect($lexer, ';');
    return $node;
}

function parse_switch_default($lexer)
{
    $node = new c_switch_default;
    expect($lexer, 'default');
    expect($lexer, ':');
    while ($lexer->more() && $lexer->peek()['kind'] != '}') {
        $node->statements[] = parse_statement($lexer);
    }
    return $node;
}
