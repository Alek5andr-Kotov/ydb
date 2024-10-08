## Именованные выражения {#named-nodes}

Сложные запросы могут выглядеть громоздко и содержать много уровней вложенности и/или повторяющихся частей. В YQL имеется возможность использовать именованные выражения – способ назначить имя произвольному выражению или подзапросу. На это именованное выражение можно ссылаться в других выражениях или подзапросах. При этом фактически происходит подстановка исходного выражения/подзапроса по месту использования.

Именованное выражение определяется следующим образом:

```antlr
<named-expr> = <expression> | <subquery>;
```

Здесь `<named-expr>` состоит из символа $ и произвольного непустого идентификатора (например `$foo`).

Если выражение в правой части представляет собой кортеж, то его можно автоматически распаковать, указав в левой части несколько именованных выражений через запятую:

```antlr
<named-expr1>, <named-expr2>, <named-expr3> ... = <expression-returning-tuple>;
```

В этом случае число выражений должно совпадать с размером кортежа.

У каждого именованного выражения есть область видимости. Она начинается сразу после определения именованного выражения и заканчивается в конце ближайшего охватывающего scope имен (например в конце запроса либо в конце тела [лямбда-функции](../../../syntax/expressions.md#lambda), [ACTION](../../action.md#define-action){% if feature_subquery %}, [SUBQUERY](../../subquery.md#define-subquery){% endif %}{% if feature_mapreduce %} или цикла [EVALUATE FOR](../../action.md#evaluate-for){% endif %}).
Повторное определение именованного выражения с тем же именем приводит к сокрытию предыдущего выражения из текущей области видимости.

Если именованное выражение ни разу не использовалось, то генерируется предупреждение. Для того, чтобы избавиться от такого предупреждения, достаточно использовать символ подчеркивания в качестве первого символа идентификатора (например `$_foo`).
Именованное выражение `$_` называется анонимным именованным выражением и обрабатывается специальным образом: оно работает, как если бы `$_` автоматически заменялось на `$_<some_uniq_name>`.
Анонимными именованными выражениями удобно пользоваться в тех случаях, когда мы не интересуемся его значением. Например, для извлечения второго элемента из кортежа из трех элементов можно написать:

```yql
$_, $second, $_ = AsTuple(1, 2, 3);
select $second;
```

Попытка сослаться на анонимное именованное выражение приводит к ошибке:

```yql
$_ = 1;
select $_; --- ошибка: Unable to reference anonymous name $_
export $_; --- ошибка: Can not export anonymous name $_
```

{% if feature_mapreduce %}

Кроме того, нельзя импортировать именованное выражение под анонимным алиасом:

```yql
import utils symbols $sqrt as $_; --- ошибка: Can not import anonymous name $_
```

{% endif %}

Анонимные имена аргументов поддерживаются также для [лямбда-функций](../../../syntax/expressions.md#lambda), [ACTION](../../action.md#define-action){% if feature_subquery %}, [SUBQUERY](../../subquery.md#define-subquery){% endif %}{% if feature_mapreduce %} и в [EVALUATE FOR](../../action.md#evaluate-for){% endif %}.

{% note info %}

Если в результате подстановки именованных выражений в графе выполнения запроса получились полностью одинаковые подграфы, они объединяются, чтобы такой подграф выполнялся только один раз.

{% endnote %}

### Примеры

```yql
$multiplier = 712;
SELECT
  a * $multiplier, -- $multiplier is 712
  b * $multiplier,
  (a + b) * $multiplier
FROM abc_table;
```

```yql
$intermediate = (
  SELECT
    value * value AS square,
    value
  FROM my_table
);
SELECT a.square * b.value
FROM $intermediate AS a
INNER JOIN $intermediate AS b
ON a.value == b.square;
```

```yql
$a, $_, $c = AsTuple(1, 5u, "test"); -- распаковка кортежа
SELECT $a, $c;
```

```yql
$x, $y = AsTuple($y, $x); -- swap значений выражений
```
