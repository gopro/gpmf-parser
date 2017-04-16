# Guidelines to write code

The ForgeJS project uses the [JSHint](http://jshint.com/) and [JSCS](http://jscs.info/) to maintain a level on consistency and compliancy across all files and the [Google Closure Compiler](https://github.com/google/closure-compiler) (GCC) to minify and optimize the build version of the library. In order to be compatible with them, a few things need to be respected when writing code.

## JSHint Compliance

To maintain a level on consistency across all files, submissions should pass the JSHint task.

The jshint and jscs tasks in Grunt tasks uses restrictive settings. Please ensure that `grunt jshint` and `grunt jscs` passes (or use `grunt lint` to run the two tasks).

## Typing and JSDoc

To optimize, the compiler relies heavily on property type. A property type, parameter or even a return value, can be typed using the JSDoc. A list of all annotations supported by the compiler is [available here](https://github.com/google/closure-compiler/wiki/Annotating-JavaScript-for-the-Closure-Compiler).

### Annotations

The following annotations are those which matter the most when using the compiler.

#### Inside the main code (`src/`)

* __`@type {}`__: goes right before a variable declaration - [JSDoc ref](http://usejsdoc.org/tags-type.html)
* __`@param {}`__: specifies the type of function parameter - [JSDoc ref](http://usejsdoc.org/tags-param.html)
* __`@return {}`__: specifies a returned value type - [JSDoc ref](http://usejsdoc.org/tags-returns.html)

#### Inside the externs (`externs/`)

* __`@typedef {{}}`__: used for new type definition (not necessary for a class, only for an Object structure) - [JSDoc ref](http://usejsdoc.org/tags-typedef.html)
* __`@property {}`__: specifies a property of a custom type - [JSDoc ref](http://usejsdoc.org/tags-property.html)

### Available types

All built-in types can be used in the JSDoc, along with all declared types of ForgeJS. But there is a trap with the built-in types: a `number` is not the same as a `Number` (note the capitalized N). The first one is only a value, while the latter is an Object. They don't represent at all the same thing for the compiler, so be aware of capitalized type. This constraint don't exist with the ForgeJS objects.

If a type is missing, like a custom Object (for example, special JSON format), it can be declared as follows :

```js
/**
 * @typedef {{foo:string, bar:AnotherType}}
 * @name {MissingType}
 * @property {string} foo
 * @property {AnotherType} bar
 */
var MissingType; // This is very important
```

A missing type can be added in the appropriate file in `externs/forge`. In order to easily find each type, they are separated across multiple files, each file corresponding to a package. For example, all custom types used in `core` are in `core.ext.js`. If the file doesn't exist yet, feel free to create it.

## Reference

Everytime the structure of the `config.json` changes, it needs to be updated in the appropriate file in `reference/`. It can be either adding new properties to an object, or adding a new object. This reference follows the [JSON Schema](http://json-schema.org/), a xsd-like document to describe and annotate JSON documents.

### Describing a property

In the field `properties` of the object to edit, there are an object for each property :

```json
"properties": {
    "foo": {
        "type": "string",
        "title": "Foo",
        "description": "The property foo, doing something.",
        "example": "foo-string-content"
    },

    "bar": {
        "type": "number",
        "title": "Bar",
        "description": "The property bar, defining something if foo is longer than 6 characters.",
        "example": 42
    }
}
```

Beside the key of the property, corresponding to the one that will be present in the `config.json` file, a property is constitued essentially of these four properties :

- `type` can be `string`, `number`, `boolean`, or more complex values
- `title` is the name of the property appearing in the documentation
- `description` is the description of the property appearing in the documentation
- `example` is the example of the property appearing in the documentation

There can be more properties, such as a `default` value, and a `min` and `max` value for numbers. For more properties, see the current reference and consult [examples on the JSON Schema website](http://json-schema.org/examples.html).

### Creating a new object

Not all properties are a string, a number or a boolean. Sometimes we need to specify complex types. Instead of simply put `object` as the type, a new object will be specified.

A new object is added to its relevant folder, in `reference/`. The structure is the following:

```json
{
    "id": "foo",
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "FooConfig",
    "description": "An instance of <a href=\"#foo-id\">a foo.</a>",
    "type": "object",

    "properties": {}
}
```

- `id` is the identifier of the object
- `$schema` is the JSON Schema reference
- `title` is the name of the object, as used in the sources (with the `@type` tag)
- `description` is the description appearing in the documention
- `type` is always `object`

Properties are then added as described above.

#### Referencing the new object

When describing a property being another object, add the reference as follows, with `$ref` equals to the `id` of the object. There is no need to specify anything more :

```json
"bar": {
    "$ref": "foo"
}
```






## Others recommandations

### Inline implicit cast

Sometimes, the compiler doesn't recognize the type of a variable, especially following conditions. To force an inline (*in a function, outside of the jsdoc block*) implicit (*force it*) cast, **wrap the member with parenthesis**, and put a `@type` tag in front of it, like this :

```js
var hello = /** @type {string} */ ( foo.bar() );
```

#### Example

Let's say a method takes one optional argument, a number. In this method, this argument is used in a function call, but this function can not take an `undefined` argument. To counter that, a check is done on it, the argument takes a default value if undefined.

```js
/** @type {string} */
var default_value = "default";

/**
 * @param {string=} optional_arg
 */
function method(optional_arg)
{
    if (optional_arg === undefined)
    {
        optional_arg = default_value;
    }

    other_function(optional_arg);
};

/**
 * @param {string} arg
 */
function other_function(arg)
{
    ...
}
```

In this case, everything seems correct, but when calling `other_function`, the compiler thinks `optional_arg` can still be `undefined`. In many case it works perfectly, but sometimes not, so an inline cast needs to be done when calling `other_function`.

```js
other_function( /** @type {string} */ (optional_arg) )
```

Apart from when no other solution can be found, try to avoid inline casting.

### Externs

Since no good externs for THREE, Hammer and Dash.js could be found, the ones used here are custom. Only the used methods and class are written here, so be sure to complete it if something is missing from it. Those extern files can be found in `externs/lib/`, with the extension `ext.js`. Below are examples how to complete them.

#### Constant

Never assign a value to a constant.

```js
/** @const */
THREE.MyConstant;
```

#### Class

```js
/**
 * @constructor
 * @param {string} arg1
 * @param {number=} arg2
 * @return {!THREE.MyClass}
 */
THREE.MyClass = function(arg1, arg2) {};
```

#### Property

Never assign a value to a property as well.

```js
/** @type {number} */
THREE.MyClass.prototype.prop;
```

#### Method

```js
/**
 * @param {(number|THREE.MyClass)} arg1
 * @param {number=} arg2
 * @param {number=} arg3
 * @return {THREE.MyClass}
 */
THREE.MyClass.prototype.myMethod = function(arg1, arg2, arg3) {};
```

### `Array` and `Map`

`Array` should always be typed, as `Array<string>`, never `Array` alone. As for the `Map` type, even if it does not exist, another type can be declared: `Object<string, FORGE.Foo>`.

### Creating a new class

When creating a new class, the most important annotation is `@constructor`. The other, if the class inherits from another, is `@extends`. Something like this should be obtained :

```js
/**
 * @constructor FORGE.Foo
 * @extends {FORGE.Bar}
 */
FORGE.Foo = function()
{
    do_something();

    FORGE.Bar.call(this);
}

FORGE.Foo.prototype = Object.create(FORGE.Bar.prototype);
FORGE.Foo.prototype.constructor = FORGE.Foo;
```

### Putting code aside

Do not comment big chunk of code, as "archive" or "backup". Either use the `@deprecated` annotation, or [remove it](http://nedbatchelder.com/text/deleting-code.html), git is here for the backup. Commented chunks like those can break the build, as used RegExp are not perfect.

### `Object.defineProperty` and `@this`

When using the `Object.defineProperty` on a class, in case the `this` keyword is used, the `getter` and `setter` should be preceded by the `@this` annotation like this, mentionning the class the property is being defined on :

```js
/**
 * Get and set the bar property.
 * @name FORGE.Foo#bar
 * @type {string}
 */
Object.defineProperty(FORGE.Foo.prototype, "bar",
{
    /** @this {FORGE.Foo} */
    get: function()
    {
        return this._bar;
    },

    /** @this {FORGE.Foo} */
    set: function(value)
    {
        this._bar = value;
    }
});
```

### Singleton

Singletons need to be avoided at all cost, but if there is no other way around it, it needs to follow this special syntax (even with the `tmp` part):

```js
/**
 * @constructor
 * @extends {FORGE.Bar}
 */
FORGE.Foo = (function(c)
{
    var tmp = c();
    tmp.prototype = Object.create(FORGE.Bar.prototype);
    tmp.prototype.constructor = tmp;

    // Prototype is then declared here
    tmp.prototype.method = function(args)
    {
        dosomething();
    };

    // Return our new instance to be affected to FORGE.Foo
    return new tmp();
})(function()
{
    return function()
    {
        // Property of the singleton are declared here
        this.prop = null;

        FORGE.Bar.call(this);
    }
});
```

### `@suppress`

The `@suppress` annotation should be avoided at all cost: if the compiler is generating a warning, there is a good reason for it.

### `Object` declaration

If an `Object` is declared in the following style, it should have all of his properties name without any quotes.

The compiler can respect the Object structure sometimes (like if the Object is simple as `{ x: 54, y: 879 }`), but the better thing to do is to provide a definition of the `Object` and type it with it.

```js
/** @type {CustomFooType} */
var Foo =
{
    bar: 2,
    baz: true
};
```

### `for...in` and `Array`

The use of such loop on an `Array` is strongly unrecommended, for a lot of reason, including the fact that the variable (`i` below) used as "index" of the array is in fact a `string`, and thus the compiler does not agree with that.

```js
for (var i in array)
{
    do_something(array[i]); // Warning, i is of type string while type number is required
}
```
[Read here for more information](http://stackoverflow.com/questions/500504/why-is-using-for-in-with-array-iteration-a-bad-idea).
