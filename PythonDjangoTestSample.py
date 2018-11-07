class TestMyService(ResponsesMixin, TestCase):


    fixtures = ["products.yaml",
                "test_my_service.yaml",
                "test_user_tickets.yaml"]

    def setUp(self):
        """Setup the database and API URL responses
        :return: None
        """
        super(TestMyService, self).setUp()

        self.setup_database()
        self.setup_service_responses()

    @staticmethod
    def setup_service_responses():
        """
        Set up the captive responses for the API URL calls.
        :return: None
        """

        from .data.test_my_service_data import \
            URL_1, RESPONSE_1, \
            URL_2, RESPONSE_2

        # for packet_count in range(len(RESPONSE_1)):
        responses.add(
            responses.GET,
            URL_1,
            json=RESPONSE_1,
            content_type='application/json')

        # for packet_count in range(len(RESPONSE_2)):
        responses.add(
            responses.GET,
            URL_2,
            json=RESPONSE_2,
            content_type='application/json')

    def perform_assert_equal(self, found, expected, text):
        """Local method to shorten the repeat code checking of table record counts
        :returns: None
        """
        self.assertEqual(found, expected, text.format(found, expected))

    def perform_true_false_assert(self, rec_id, expected_result, expected_error):
        """Local method to shorten the repeat code checking Result and Error for True/False
        :returns: None
        """
        test_record = LookupResult.objects.get(id=rec_id)

        if test_record:
            if expected_result:
                self.assertTrue(test_record.result,
                                'lookup_result(id={0}) has initial result=False, expected True'.format(rec_id))
            else:
                self.assertFalse(test_record.result,
                                 'lookup_result(id={0}) has initial result=True, expected False'.format(rec_id))

            if expected_error:
                self.assertTrue(test_record.error,
                                'lookup_result(id={0}) has initial error=False, expected True'.format(rec_id))
            else:
                self.assertFalse(test_record.error,
                                 'lookup_result(id={0}) has initial error=True, expected False'.format(rec_id))
        else:
            self.assertTrue(False, 'lookup_result(id={0}) record cannot be located'.format(rec_id))

    def setup_database(self):
        """
        The database is loaded by fixtures, so this method just verifies that the data in
        the tables looks like what we expect before we run any test(s) against it.
        :return: None
        """

        from local_app.test.data.test_my_service_data import \
            EXPECTED_DB_FUNC_RECS, \
            EXPECTED_LOOKUP_RECS, \
            EXPECTED_RELEASED_RECS, \
            EXPECTED_USER_TKT_MIN_RECS, \
            INDEXED_ID_LIST, \
            PRODUCT_ID_LIST

        log.info('Checking test databases for expected TestMyService() fixture data')

        # Check DbFuncCall table
        self.perform_assert_equal(DbFuncCall.objects.all().count(), EXPECTED_DB_FUNC_RECS,
                             ‘DbFuncCall table has {0} total records, expected {1}')

        # Check LookupResults table
        self.perform_assert_equal(LookupResult.objects.all().count(), EXPECTED_LOOKUP_RECS,
                             'lookup_result test table has {0} total records, expected {1}')

       # Check user tickets
        nbr_user_tkt_recs = UserTickets.objects.all().count()
        self.assertTrue(nbr_user_tkt_recs >= EXPECTED_USER_TKT_MIN_RECS,
                        user_tkt test table has {0} total records, expected at least {1}'
                        .format(nbr_user_tkt_recs, EXPECTED_USER_TKT_MIN_RECS))

        # Check Product table
        for product in PRODUCT_ID_LIST:
            self.assertEqual(Product.objects.filter(id=product).count(), 1,
                             'Product(id={0}) does not exist in test database'.format(product))

        # Check lookup_result initial data settings
        self.perform_true_false_assert(rec_id=1, expected_result=False, expected_error=True)
        self.perform_true_false_assert(rec_id=2, expected_result=True, expected_error=True)
        self.perform_true_false_assert(rec_id=3, expected_result=False, expected_error=True)

    def asserts_after_updates(self):
        """
        Verifies that the database is in the expected state for the test my_service call.

        Note: In actual use, an unexpected LookupResult.error=True would not be viewed as an
        my_service() failure, but we're using fake API calls with fake responses so it's not
        possible to get an unexpected URL API error. The flags should always be what we expect for the tests.

        :return: None
        """

        # Check lookup_result expected data results
        self.perform_true_false_assert(rec_id=1, expected_result=True, expected_error=False)
        self.perform_true_false_assert(rec_id=2, expected_result=True, expected_error=True)
        self.perform_true_false_assert(rec_id=3, expected_result=True, expected_error=False)

    def test_my_service_run1(self):
        """
        Call my_service method using mock Pisces URLs and responses, then check that the table changes
        meet expected results.
        :return: None
        """

        my_service()
        self.asserts_after_updates()


