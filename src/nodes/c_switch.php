<?php

class c_switch
{
    private $value;
    private $cases = [];
    private $default;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'switch');
        expect($lexer, '(');
        $self->value = parse_expression($lexer);
        expect($lexer, ')');
        expect($lexer, '{');
        while ($lexer->follows('case')) {
            $self->cases[] = c_switch_case::parse($lexer);
        }
        if ($lexer->follows('default')) {
            $self->default = c_switch_default::parse($lexer);
        }
        expect($lexer, '}');
        return $self;
    }

    function format()
    {
        $s = '';
        foreach ($this->cases as $case) {
            $s .= $case->format() . "\n";
        }
        if ($this->default) {
            $s .= $this->default->format() . "\n";
        }
        return sprintf(
            "switch (%s) {\n%s\n}",
            $this->value->format(),
            indent($s)
        );
    }
}
