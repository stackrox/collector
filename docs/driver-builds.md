### CPaaS drivers flow

<img align="center" src="./imgs/cpaas-driver.svg">

<details>
<summary>raw plantuml</summary>

```
@startuml

frame "GCP" {
    component [CPaaS driver cache] as dcache
    component [Support package cache] as supabucket
}

frame "CPaaS" {
    component [Brew Registry] as brewreg

    [Content Sets] --> Brew
    [MidStream Repo] --> Brew
    Brew -> brewreg

    note left of Brew: builds driver image
}

frame "OSCI" {
    component [Internal Registry] as oscireg

    frame images {
        component [cpaas-drivers] as cpaasdrivers
    }

    frame tests {
        component [Push drivers &\nbuild support package] as buildandsupa
        component [Test Drivers Compilation] as dtests
    }

    brewreg --> oscireg: mirrors into
    oscireg --> cpaasdrivers
    cpaasdrivers --> buildandsupa
    dcache ..> buildandsupa
    buildandsupa ..> dcache
    buildandsupa ..> supabucket
    cpaasdrivers --> dtests
}

note as driverpushpull
    New drivers are pushed.
    Drivers that are missing are pulled.
end note

driverpushpull .. buildandsupa
driverpushpull .. dcache
@enduml
```
</details>
