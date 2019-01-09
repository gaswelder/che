<?php

class c_for
{
    private $init;
    private $condition;
    private $action;
    private $body;

    static function parse($lexer)
    {
        $self = new self;
        expect($lexer, 'for');
        expect($lexer, '(');

        if (is_type($lexer->peek(), $lexer->typenames)) {
            $self->init = c_loop_counter_declaration::parse($lexer);
        } else {
            $self->init = parse_expression($lexer);
        }

        expect($lexer, ';');
        $self->condition = parse_expression($lexer);
        expect($lexer, ';');
        $self->action = parse_expression($lexer);
        expect($lexer, ')');
        $self->body = c_body::parse($lexer);
        return $self;
    }

    function format()
    {
        return sprintf(
            'for (%s; %s; %s) %s',
            format_expression($this->init),
            format_expression($this->condition),
            format_expression($this->action),
            $this->body->format()
        );
    }
}
