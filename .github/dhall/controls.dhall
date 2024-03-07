let Control =
      { Type = { name : Text, description : Text }
      , defaults = { name = "", description = "" }
      }

let controls =
        [ { name = "benchmarks_calculate-baseline_enable"
          , description = "Controls baseline step in benchmarks job"
          }
        , { name = "memory-checked-unit-tests_enable"
          , description = "Controls job memory checked unit tests"
          }
        ]
      : List Control.Type

in  { Type = Control.Type, Params = controls }
