# SimpleON - almost JSON but simplified for human

Inspired by HJSON (https://hjson.org/) but more simply designed. I create this to fit my personal needs.

## Format

Almost JSON, with certain exceptions:

1. Item separator (i.e. comma) is optional. Key-value separator (i.e. colon) is still required.

2. Unquoted string is allowed. RE: r'[^\[\]{}:", ]+'.

    Integers, floats, and booleans (true and false) will be auto-converted.

3. Multi-line strings -- start and end with """
    
4. Comment allowed. Text after '#' is ignored for each line (except when parsing quoted and multi-line strings) 

5. Unclosed quoted string is auto-closed to the end of line

## Examples

```
{
  foo : bar
  answer : 42
}
```

```
{
  foo : "hello world # will be included
  # this is a comment line
  multi-line : """
  this is a multi-line string
  # this is not a comment line
  and line 2
  """
}
```

## Usage

To install, run `python setup.py install`.

To use the module as a utility, run `python -m simpleon`. It will parse stdin as an SimpleON and output a pretty printed JSON.

To use the parser directly, see `__main__.py` for example.

## Limitations

Generally the project is in very early stage.

Slash escaping in strings is WIP.

C++ parser is WIP.

## License

MIT License - See LICENSE
