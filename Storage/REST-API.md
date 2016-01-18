# RESTful binding for Storage, RFC

This document describes the RESTful API with CRUD functionality for Current Storage.

### Bindings semantics
`TODO: Describe storage field <-> endpoint bindings.`

### Response schema

The response body is always either a valid JSON or empty.

The generic format of a successful JSON response is:

```
{
  "url": "https://api.example.com/...",
  "url_*": "https://api.example.com/...",
  ... more "url_*" fields ...
  
  [optional]
  "meta": {
    ...
  },
  
  [required]
  "data": {
    ...
  }

}
```

The responses are strongly typed. The contents of the request URL define the schema for the response.

The value for `"data"` can be:
* an object,
* an array, if the response is expected to contain a list,
* primitive type (string, number, boolean) in special cases.

Error responses are JSON objects with the top-level `"error":...` field:

```
{
  “error”: “Detailed human-readable error message.”,
  ... other fields ...
}
```

Successful responses are JSON objects without the top-level `"error"` field.

`TODO: We also may have redirects, which contain the "Location" in the header, with empty body.`

### Notation

The default way to encode JSON objects is JavaScript-friendly. The API also supports F#-friendly format.

The differences are:

1. Variant types.

   If a field `x` can be either an `apple` or an `orange`, the JavaScript-friendly representation is `{...,"x":{"apple":{...},...}`, and the F#-friendly representation is `{...,"x":{"Case":"apple","Fields":[...]},...}}`.

2. Coding of absent optional objects.

   The JavaScript-friendly implementation omits missing fields from the output (and does not expect them in the input).
The F#-friendly implementation outputs missing fields as `null`-s (and expects `null`-s in the input).

3. Coding of dictionaries with the keys being strings.

   Generally, both JavaScript- and F#-friendly implementations encode dictionaries as arrays of arrays of two elements: `[[key1,value1],[key2,value2],...]`.
   
   However, if the type of `key` is string, the JavaScript-friendly format of this dictionary becomes an object: `{"key1":value1,"key2":value2,...}`.

   `TODO(dkorolev): Check if some other F#-related issues should be added into this section.`

### Views

**View** is a representation of a record, which contains some non-empty subset of the record’s fields.
The API supports different views via the `&format=...` URL parameter.

By default, records returned by the API contain all fields.

Two implicitly defined views (both default to "all fields") are `full` and `preview`.

The `full` view is the default, so that `&format=full` is equivalent to not specifying the `&format` URL parameter.

The `&format=preview` is used when the response schema for certain endpoint contains not one record, but a collection of them. In other words, the representation of each record of `"data":[...]` for the endpoints where `"data"` is an array is equivalent to the representation of those records with `&format=preview`.

The views are defined by the user, and the default representation of any view is "all fields". In particular, unless the user defines the `"preview"` view, it will be identical to the `"full"` one.

### Methods

Current Storage API interprets HTTP verbs in the following way:

* **GET**: Get the resource.

  Strongly typed with respect to returning an individual record or a collection of records. For example, `/users` can be the way to GET the list of users (`"data":[...]`), and `/user/1` can be the way to GET an individual user with the ID of `1`.
   
* **PATCH**: Update some fields of the resource.

  `PATCH` should be called on the URL that has been (or could have been) returned as the top-level `"url":"..."` field of the payload of `GET`.

  Generally, the user is expected to retrieve the value of the object under the key `"data"` of the top-level payload of a `GET` request, alter this object, and send its modified version back to the server.

  `PATCH` respects the format of the `"data"` controlled by the `&format=` URL parameter. Sending a `PATCH` request to the URL with certain `&format=X` can only modify the fields returned by the `GET` request of this `&format=X`.

  For `PATCH` requests, the key of the record being updated is taken from the URL. The user is expected to either not send in the key as part of the payload, or ensure the key contained in the payload matches the key contained in the URL.
  
  `PATCH` will fail if attempted to modify a non-existing resource, resulting in `404 (Not Found)` error code.
  
  Also, `PATCH` performs conflict resolution. The URL being `PATCH`-ed must contain a revision / timestamp of the resource. The exact semantics of the revision / timestamp is not specified by this RFC, but the top-level `"url"` field returned by the `GET` request is guaranteed to have it set.

  An attempt to `PATCH` to a URL that points to the version of the resource other than the most recent one will result in no data mutation and a `409 Conflict` error code. The user is then expected to re-run the `GET` request, followed by the re-run `PATCH` request onto the URL of the most recent version of the resource returned by the `GET` request. Confirming that the modifications that occurred in the meantime do not conflict with the new payload being sent in the body of the `PATCH` request is then the responsibility of the API user.

* **PUT**: Create a new record for a resource, or update all fields of an existing resource.

  Attempting to `PUT` to a URL that has an incomplete view of a record would result in a `405 Method Not Allowed` error.
  
  In the `&format=full` view, `PUT` is equivalent to `PATCH`. 
  
  `PUT` behaves the same way as `PATCH` when it comes to avoiding merge conflicts.

  When legal, the only difference between `PUT` and `PATCH` is that `PUT` will create a new resource if it did not exist before the call. A `201 Created` response code will then be returned. Use `PATCH` if this behavior is undesirable.

  `TODO: Location header https://tools.ietf.org/html/rfc7231#section-4.3.3`

* **POST**: Create a new record and assign it to some recourse.

  `POST` will create a new resource. The URL for POST must not contain the key(s) for the record being posted, and the payload body should not contain the key(s) either.

  The generation of the new key is user-defined. Three common strategies are: 1) random, 2) the [salted] hash of the payload, 3) the value of certain passed in field or fields.
  
  In the latter two, as well as other, cases, a `POST` request may result in an attempt to overwrite an already existing resource. By default, `POST` will not overwrite, but return a `409 Conflict` error code, with the top-level `"url"` field of the returned JSON object containing the URL for the resource that was about to be overwritten.

  The `&overwrite=1` / `&overwrite=true` URL parameter can be set to indicate that the `POST` request should complete successfully even if it would result in overwriting existing data.

Successful `POST` results in either `200 (OK)` error code if the resource has been updated, or `201 (Created)`.

* **DELETE**: Deletes the resource.

   Similarly to `PATCH` and `PUT`, `DELETE` should be called on the URL that has been (or could have been) returned as the top-level `"url":"..."` field of the payload of `GET`.
   
   `DELETE` follows the same conflict resolution rules as `PATCH` and `PUT`.
   
`TODO: User-defined data integrity checks, and what errors are returned if they fail?`


### Discoverability

The entry point (usually, `"/"`) URL will contain the list of inner `"url_*"`-s to access respective fields ("tables") of the storage.

The entry point URL may also contain the URLs by which the user can access the respective schema files. This is applicable to F# and C++.

[ TO DISCUSS ] The payload when `GET`-ting the resource corresponding to an individual record (ex. `/user/123`) will contain the `{...,"url_collection":".../users"}` reference to the URL to browse the whole collection, the record from which is being requested.

As a record is successfully created or modified, the response payload will contain its URL as the top-level `"url"` field.

When a resource can not be found, the error payload will contain the URL to browse the respective collection.

When the operation fails due to a conflict, the top-level `"url"` field will be set as well.

### Pagination

When retrieving the collection of records, they are returned in pages. If there are more records to browse, the top-level response object will contain the `"url_next_page"` field, as well as the `"url_previous_page"` one for the pages past the first one.

As mentioned above, in the pagination mode:
* the `"data":[...]` top-level field becomes an array, not object, and
* the elements of this array default to the `&format=preview` format, if defined by the user.

With respect to pagination:
* The user SHOULD NOT attempt to skip through records (ex. browse page 1 with records 1..20, and then skip immediately to records 101..120.
* The user CAN expect the records they have already seen as part of this session will not change.
* The user SHOULD NOT expect any record they have seen or may see will exist and/or be available to them by the time the user decides to perform actions on that record.

In other words, the behavior of pagination for the user mimics the architecture where on the first `GET` request to browse the collection, the complete list of possible IDs is populated on the server, and the user is just browsing through these IDs.

The server will honor the entries the user has already browsed through, even if the user requests the previous page, and some entries from it have been deleted. At the same time, while browsing further, the server may re-fetch the records and re-apply the filters.

In particular, scanning through pages CAN return records created AFTER the time of the first `GET` request, unless the explicit filter "created before X" has been requested by the user.

The token returned by the API to page through the collection expires by itself. The default period for which the token will be live is 10 minutes since it was last used.

`TODO: Document page size and the ability to dynamically change it.`
