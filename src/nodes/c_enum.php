<?php

class c_enum_member
{
    public $id;
    public $value;

    function format()
    {
        $s = $this->id->format();
        if ($this->value) {
            $s .= ' = ' . $this->value->format();
        }
        return $s;
    }
}

class c_compat_enum
{
    private $hidden;
    private $members;

    function __construct($members, $hidden)
    {
        $this->members = $members;
        $this->hidden = $hidden;
    }

    function is_private()
    {
        return $this->hidden;
    }

    function format()
    {
        $s = "enum {\n";
        foreach ($this->members as $i => $member) {
            if ($i > 0) {
                $s .= ",\n";
            }
            $s .= "\t" . $member->format();
        }
        $s .= "\n};\n";
        return $s;
    }
}

class c_enum
{
    public $members = [];
    public $pub;

    function format()
    {
        $s = '';
        if ($this->pub) {
            $s .= 'pub ';
        }
        $s .= "enum {\n";
        foreach ($this->members as $id) {
            $s .= "\t" . $id->format() . ",\n";
        }
        $s .= "}\n";
        return $s;
    }

    function translate()
    {
        return new c_compat_enum($this->members, !$this->pub);
    }
}
