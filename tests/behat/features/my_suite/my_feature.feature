@example
Feature: Example Feature
  Go to an admin page and see that it loads.

  Scenario: Go to homepage.
    Given I am logged in as a user with the "developer" role
    When I go to "/admin/config"
    Then I should see "Logging and errors"
    And I should see "Maintenance mode"
