#!/usr/bin/env -S dhall-to-yaml --explain --file

let List/map =
      https://prelude.dhall-lang.org/v11.1.0/List/map
        sha256:dd845ffb4568d40327f2a817eb42d1c6138b929ca758d50bc33112ef3c885680

let GithubActions = https://regadas.dev/github-actions-dhall/package.dhall

let Controls = ./controls.dhall

-- Generate step saving the value to GITHUB_OUTPUT
let setValue =
      \(control : Controls.Type) ->
        GithubActions.Step::{
        , id = Some "set_${control.name}"
        , name = None Text
        , run = Some
            ''
            if [[ "''${{ contains(github.event.pull_request.labels.*.name, '${control.name}') }}" == "true" ]]; then
                echo "${control.name}=true" > "''${GITHUB_OUTPUT}"
            fi
            ''
        , uses = None Text
        }

-- Generate workflow output containing the control variables
let workflowOutput =
      \(control : Controls.Type) ->
        { mapKey = control.name
        , mapValue = GithubActions.Output::{
          , value = "jobs.control-vars.outputs.${control.name}"
          , description = Some "${control.description}"
          }
        }

-- Generate job output containing the control variables
let jobOutput =
      \(control : Controls.Type) ->
        { mapKey = control.name
        , mapValue = "\${{ steps.set_${control.name}.outputs.${control.name} }}"
        }

-- Prepare both workflow and job outputs containing all control variables
let workflowCall =
      GithubActions.WorkflowCall::{
      , outputs = Some
          ( List/map
              Controls.Type
              { mapKey : Text, mapValue : GithubActions.Output.Type }
              workflowOutput
              Controls.Params
          )
      }

let jobOutputs =
      List/map
        Controls.Type
        { mapKey : Text, mapValue : Text }
        jobOutput
        Controls.Params

in  GithubActions.Workflow::{
    , name = "Init control variables"
    , on = GithubActions.On::{ workflow_call = Some workflowCall }
    , jobs = toMap
        { control-vars = GithubActions.Job::{
          , name = Some "Collect control variables"
          , runs-on = GithubActions.RunsOn.Type.ubuntu-latest
          , outputs = Some jobOutputs
          , steps =
              List/map
                Controls.Type
                GithubActions.Step.Type
                setValue
                Controls.Params
          }
        }
    }
